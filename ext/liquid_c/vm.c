#include <stdint.h>

#include "liquid.h"
#include "vm.h"
#include "resource_limits.h"
#include "context.h"

ID id_render_node;
ID id_ivar_interrupts;
ID id_ivar_resource_limits;
ID id_to_s;
ID id_vm;
ID id_strainer;
ID id_filter_methods_hash;
ID id_strict_filters;
ID id_global_filter;

static VALUE cLiquidCVM;

typedef struct vm {
    c_buffer_t stack;
    VALUE strainer;
    VALUE filter_methods;
    VALUE interrupts;
    VALUE resource_limits_obj;
    resource_limits_t *resource_limits;
    VALUE global_filter;
    bool strict_filters;
} vm_t;

static void vm_mark(void *ptr)
{
    vm_t *vm = ptr;
    rb_gc_mark_locations((VALUE *)vm->stack.data, (VALUE *)vm->stack.data_end);
    rb_gc_mark(vm->strainer);
    rb_gc_mark(vm->filter_methods);
    rb_gc_mark(vm->interrupts);
    rb_gc_mark(vm->resource_limits_obj);
    rb_gc_mark(vm->global_filter);
}

static void vm_free(void *ptr)
{
    vm_t *vm = ptr;
    c_buffer_free(&vm->stack);
    xfree(vm);
}

static size_t vm_memsize(const void *ptr)
{
    const vm_t *vm = ptr;
    return sizeof(vm_t) + c_buffer_capacity(&vm->stack);
}

const rb_data_type_t vm_data_type = {
    "liquid_vm",
    { vm_mark, vm_free, vm_memsize, },
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE vm_internal_new(VALUE context)
{
    vm_t *vm;
    VALUE obj = TypedData_Make_Struct(cLiquidCVM, vm_t, &vm_data_type, vm);
    vm->stack = c_buffer_init();

    vm->strainer = rb_funcall(context, id_strainer, 0);
    Check_Type(vm->strainer, T_OBJECT);

    vm->filter_methods = rb_funcall(RBASIC_CLASS(vm->strainer), id_filter_methods_hash, 0);
    Check_Type(vm->filter_methods, T_HASH);

    vm->interrupts = rb_ivar_get(context, id_ivar_interrupts);
    Check_Type(vm->interrupts, T_ARRAY);

    vm->resource_limits_obj = rb_ivar_get(context, id_ivar_resource_limits);;
    ResourceLimits_Get_Struct(vm->resource_limits_obj, vm->resource_limits);

    vm->strict_filters = RTEST(rb_funcall(context, id_strict_filters, 0));
    vm->global_filter = rb_funcall(context, id_global_filter, 0);
    return obj;
}

static vm_t *vm_from_context(VALUE context)
{
    VALUE vm_obj = rb_attr_get(context, id_vm);
    if (vm_obj == Qnil) {
        vm_obj = vm_internal_new(context);
        rb_ivar_set(context, id_vm, vm_obj);
    }
    // instance variable is hidden from ruby so should be safe to unwrap it without type checking
    return DATA_PTR(vm_obj);
}

static void write_fixnum(VALUE output, VALUE fixnum)
{
    long long number = RB_NUM2LL(fixnum);
    int write_length = snprintf(NULL, 0, "%lld", number);
    long old_size = RSTRING_LEN(output);
    long new_size = old_size + write_length;
    long capacity = rb_str_capacity(output);

    if (new_size > capacity) {
        do {
            capacity *= 2;
        } while (new_size > capacity);
        rb_str_resize(output, capacity);
    }
    rb_str_set_len(output, new_size);

    snprintf(RSTRING_PTR(output) + old_size, write_length + 1, "%lld", number);
}

static void write_obj(VALUE output, VALUE obj)
{
    switch (TYPE(obj)) {
        case T_STRING:
            rb_str_buf_append(output, obj);
            break;
        case T_FIXNUM:
            write_fixnum(output, obj);
            break;
        case T_ARRAY:
            for (long i = 0; i < RARRAY_LEN(obj); i++)
            {
                VALUE item = RARRAY_AREF(obj, i);

                if (RB_UNLIKELY(RB_TYPE_P(item, T_ARRAY))) {
                    // Normally liquid arrays are flat, but for safety and simplicity we
                    // leverage ruby's join that detects and raises on a recursion loop
                    rb_str_buf_append(output, rb_ary_join(item, Qnil));
                } else {
                    write_obj(output, item);
                }
            }
            break;
        case T_NIL:
            break;
        default:
            obj = rb_funcall(obj, id_to_s, 0);
            rb_str_append(output, obj);
            break;
    }
}

static inline void vm_stack_push(vm_t *vm, VALUE value)
{
    VALUE *stack_ptr = (VALUE *)vm->stack.data_end;
    *stack_ptr++ = value;
    vm->stack.data_end = (uint8_t *)stack_ptr;
}

static inline VALUE vm_stack_pop(vm_t *vm)
{
    VALUE *stack_ptr = (VALUE *)vm->stack.data_end;
    stack_ptr--;
    vm->stack.data_end = (uint8_t *)stack_ptr;
    return *stack_ptr;
}

static inline VALUE *vm_stack_pop_n_use_in_place(vm_t *vm, size_t n)
{
    VALUE *stack_ptr = (VALUE *)vm->stack.data_end;
    stack_ptr -= n;
    vm->stack.data_end = (uint8_t *)stack_ptr;
    return stack_ptr;
}

static inline void vm_stack_reserve_for_write(vm_t *vm, size_t num_values)
{
    c_buffer_reserve_for_write(&vm->stack, num_values * sizeof(VALUE));
}

static VALUE vm_invoke_filter(vm_t *vm, VALUE filter_name, int num_args, VALUE *args)
{
    bool not_invokable = rb_hash_lookup(vm->filter_methods, filter_name) != Qtrue;
    if (RB_UNLIKELY(not_invokable)) {
        if (vm->strict_filters) {
            VALUE error_class = rb_const_get(mLiquid, rb_intern("UndefinedFilter"));
            rb_raise(error_class, "undefined filter %"PRIsVALUE, rb_sym2str(filter_name));
        }
        return args[0];
    }

    VALUE result = rb_funcallv(vm->strainer, RB_SYM2ID(filter_name), num_args, args);
    return rb_funcall(result, id_to_liquid, 0);
}

typedef struct vm_render_until_error_args {
    vm_t *vm;
    const uint8_t *ip; // use for initial address and to save an address for rescuing
    const size_t *const_ptr;
    unsigned int node_line_number;
    VALUE context;
    VALUE output;
} vm_render_until_error_args_t;

#ifdef HAVE_RB_HASH_BULK_INSERT
#define hash_bulk_insert rb_hash_bulk_insert
#else
static void hash_bulk_insert(long argc, const VALUE *argv, VALUE hash)
{
    for (long i = 0; i < argc; i += 2) {
        rb_hash_aset(hash, argv[i], argv[i + 1]);
    }
}
#endif

// Actually returns a bool resume_rendering value
static VALUE vm_render_until_error(VALUE uncast_args)
{
    vm_render_until_error_args_t *args = (void *)uncast_args;
    const size_t *const_ptr = args->const_ptr;
    const uint8_t *ip = args->ip;
    vm_t *vm = args->vm;
    VALUE output = args->output;
    args->ip = NULL; // used by vm_render_rescue, NULL to indicate that it isn't in a rescue block

    while (true) {
        switch (*ip++) {
            case OP_LEAVE:
                return false;

            case OP_PUSH_CONST:
                vm_stack_push(vm, (VALUE)*const_ptr++);
                break;
            case OP_HASH_NEW:
            {
                size_t hash_size = *ip++;
                size_t num_keys_and_values = hash_size * 2;
                VALUE hash = rb_hash_new();
                VALUE *args_ptr = vm_stack_pop_n_use_in_place(vm, num_keys_and_values);
                hash_bulk_insert(num_keys_and_values, args_ptr, hash);
                vm_stack_push(vm, hash);
                break;
            }
            case OP_FILTER:
            {
                VALUE filter_name = (VALUE)*const_ptr++;
                uint8_t num_args = *ip++; // includes input argument
                VALUE *args_ptr = vm_stack_pop_n_use_in_place(vm, num_args);
                VALUE result = vm_invoke_filter(vm, filter_name, num_args, args_ptr);
                vm_stack_push(vm, result);
                break;
            }
            case OP_PUSH_EVAL_EXPR:
            {
                VALUE expression = (VALUE)*const_ptr++;
                vm_stack_push(vm, context_evaluate(args->context, expression));
                break;
            }

            // Rendering instructions

            case OP_WRITE_RAW:
            {
                const char *text = (const char *)*const_ptr++;
                size_t size = *const_ptr++;
                rb_str_cat(output, text, size);
                resource_limits_increment_write_score(vm->resource_limits, output);
                break;
            }
            case OP_WRITE_NODE:
                rb_funcall(cLiquidBlockBody, id_render_node, 3, args->context, output, (VALUE)*const_ptr++);
                if (RARRAY_LEN(vm->interrupts)) {
                    return false;
                }
                resource_limits_increment_write_score(vm->resource_limits, output);
                break;
            case OP_RENDER_VARIABLE_RESCUE:
                // Save state used by vm_render_rescue to rescue from a variable rendering exception
                args->node_line_number = (unsigned int)*const_ptr++;
                // vm_render_rescue will iterate from this instruction to the instruction
                // following OP_POP_WRITE_VARIABLE to resume rendering from
                args->ip = ip;
                args->const_ptr = const_ptr;
                break;
            case OP_POP_WRITE_VARIABLE:
            {
                VALUE var_result = vm_stack_pop(vm);
                if (vm->global_filter != Qnil)
                    var_result = rb_funcall(vm->global_filter, id_call, 1, var_result);
                write_obj(output, var_result);
                args->ip = NULL; // mark the end of a rescue block, used by vm_render_rescue
                resource_limits_increment_write_score(vm->resource_limits, output);
                break;
            }

            default:
                rb_bug("invalid opcode: %u", ip[-1]);
        }
    }
}

void liquid_vm_next_instruction(const uint8_t **ip_ptr, const size_t **const_ptr_ptr)
{
    const uint8_t *ip = *ip_ptr;

    switch (*ip++) {
        case OP_LEAVE:
        case OP_POP_WRITE_VARIABLE:
            break;

        case OP_HASH_NEW:
            ip++;
            break;

        case OP_WRITE_NODE:
        case OP_PUSH_CONST:
        case OP_PUSH_EVAL_EXPR:
        case OP_RENDER_VARIABLE_RESCUE:
            (*const_ptr_ptr)++;
            break;

        case OP_FILTER:
            ip++;
            (*const_ptr_ptr)++;
            break;

        case OP_WRITE_RAW:
            (*const_ptr_ptr) += 2;
            break;

        default:
            rb_bug("invalid opcode: %u", ip[-1]);
    }
    *ip_ptr = ip;
}

typedef struct vm_render_rescue_args {
    vm_render_until_error_args_t *render_args;
    size_t old_stack_byte_size;
} vm_render_rescue_args_t;

// Actually returns a bool resume_rendering value
static VALUE vm_render_rescue(VALUE uncast_args, VALUE exception)
{
    vm_render_rescue_args_t *args = (void *)uncast_args;
    VALUE blank_tag = Qfalse; // tags are still rendered using Liquid::BlockBody.render_node
    vm_render_until_error_args_t *render_args = args->render_args;

    const uint8_t *ip = render_args->ip;
    if (ip) {
        // rescue for variable render, where ip is at the start of the render and we need to
        // skip to the end of the variable render to resume rendering if the error is handled
        enum opcode last_op;
        do {
            last_op = *ip;
            liquid_vm_next_instruction(&ip, &render_args->const_ptr);
        } while (last_op != OP_POP_WRITE_VARIABLE);
        render_args->ip = ip;
        vm_t *vm = render_args->vm;
        // remove temporary stack values from variable evaluation
        vm->stack.data_end = vm->stack.data + args->old_stack_byte_size;
    }

    VALUE line_number = render_args->node_line_number == 0 ? Qnil : UINT2NUM(render_args->node_line_number);

    rb_funcall(cLiquidBlockBody, rb_intern("c_rescue_render_node"), 5,
        render_args->context, render_args->output, line_number, exception, blank_tag);
    return true;
}

void liquid_vm_render(block_body_t *body, VALUE context, VALUE output)
{
    vm_t *vm = vm_from_context(context);

    vm_stack_reserve_for_write(vm, body->code.max_stack_size);
    resource_limits_increment_render_score(vm->resource_limits, body->render_score);

    vm_render_until_error_args_t render_args = {
        .vm = vm,
        .const_ptr = (const size_t *)body->code.constants.data,
        .ip = body->code.instructions.data,
        .context = context,
        .output = output,
    };
    vm_render_rescue_args_t rescue_args = {
        .render_args = &render_args,
        .old_stack_byte_size = c_buffer_size(&vm->stack),
    };

    while (rb_rescue(vm_render_until_error, (VALUE)&render_args, vm_render_rescue, (VALUE)&rescue_args)) {
    }
}


void init_liquid_vm()
{
    id_render_node = rb_intern("render_node");
    id_ivar_interrupts = rb_intern("@interrupts");
    id_ivar_resource_limits = rb_intern("@resource_limits");
    id_to_s = rb_intern("to_s");
    id_vm = rb_intern("vm");
    id_strainer = rb_intern("strainer");
    id_filter_methods_hash = rb_intern("filter_methods_hash");
    id_strict_filters = rb_intern("strict_filters");
    id_global_filter = rb_intern("global_filter");

    cLiquidCVM = rb_define_class_under(mLiquidC, "VM", rb_cObject);
    rb_undef_alloc_func(cLiquidCVM);
    rb_global_variable(&cLiquidCVM);
}
