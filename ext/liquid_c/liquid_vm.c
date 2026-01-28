#include <stdint.h>
#include <assert.h>

#include "liquid.h"
#include "liquid_vm.h"
#include "variable_lookup.h"
#include "intutil.h"
#include "document_body.h"

ID id_render_node;
ID id_vm;
static ID id_to_liquid_value;

static VALUE cLiquidCVM;

/* Cached Ruby classes for native tag optimization */
static VALUE cLiquidIncrement = Qnil;
static VALUE cLiquidDecrement = Qnil;
static VALUE cLiquidComment = Qnil;
static ID id_variable_name;

/* Singletons for blank/empty keyword comparisons */
static VALUE blank_singleton = Qnil;
static VALUE empty_singleton = Qnil;

static void vm_mark(void *ptr)
{
    vm_t *vm = ptr;

    c_buffer_rb_gc_mark(&vm->stack);
    context_mark(&vm->context);
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

/* Check if a value is considered "empty" in Liquid.
 * Empty values: empty strings, empty arrays, and empty hashes.
 * Note: nil and false are NOT empty (use blank for those).
 */
static bool is_value_empty(VALUE val)
{
    if (RB_TYPE_P(val, T_STRING)) {
        return RSTRING_LEN(val) == 0;
    }

    if (RB_TYPE_P(val, T_ARRAY)) {
        return RARRAY_LEN(val) == 0;
    }

    if (RB_TYPE_P(val, T_HASH)) {
        return RHASH_SIZE(val) == 0;
    }

    return false;
}

/* Check if a value is considered "blank" in Liquid.
 * Blank values: nil, false, empty strings, whitespace-only strings,
 * empty arrays, and empty hashes.
 */
/* Unwrap a drop value by calling to_liquid_value if it responds to it.
 * This is used for comparisons and truthiness checks to get the underlying value.
 */
static VALUE unwrap_drop_value(VALUE val)
{
    VALUE unwrapped = rb_check_funcall(val, id_to_liquid_value, 0, 0);
    if (unwrapped != Qundef) {
        return unwrapped;
    }
    return val;
}

static bool is_value_blank(VALUE val)
{
    if (val == Qnil || val == Qfalse) {
        return true;
    }

    if (RB_TYPE_P(val, T_STRING)) {
        const char *ptr = RSTRING_PTR(val);
        long len = RSTRING_LEN(val);

        /* Check if empty or all whitespace */
        for (long i = 0; i < len; i++) {
            if (!rb_isspace(ptr[i])) {
                return false;
            }
        }
        return true;
    }

    if (RB_TYPE_P(val, T_ARRAY)) {
        return RARRAY_LEN(val) == 0;
    }

    if (RB_TYPE_P(val, T_HASH)) {
        return RHASH_SIZE(val) == 0;
    }

    return false;
}

/* Helper for blank/empty-aware equality comparison.
 * When either operand is the blank or empty singleton, check if the other value is blank/empty.
 * This matches Ruby Liquid's MethodLiteral behavior.
 */
static VALUE vm_equal_variables(VALUE a, VALUE b)
{
    /* Check for empty singleton */
    if (empty_singleton != Qnil) {
        if (a == empty_singleton) {
            return is_value_empty(b) ? Qtrue : Qfalse;
        }
        if (b == empty_singleton) {
            return is_value_empty(a) ? Qtrue : Qfalse;
        }
    }

    /* Check for blank singleton */
    if (blank_singleton != Qnil) {
        if (a == blank_singleton) {
            return is_value_blank(b) ? Qtrue : Qfalse;
        }
        if (b == blank_singleton) {
            return is_value_blank(a) ? Qtrue : Qfalse;
        }
    }
    return rb_equal(a, b) ? Qtrue : Qfalse;
}

/*
 * For loop iterator state.
 * Stored on the VM stack as a Ruby Array: [items, index, length, var_name, forloop_drop, parent_forloop]
 * This allows GC to properly track all values.
 */
#define FORLOOP_STATE_ITEMS     0
#define FORLOOP_STATE_INDEX     1
#define FORLOOP_STATE_LENGTH    2
#define FORLOOP_STATE_VAR_NAME  3
#define FORLOOP_STATE_DROP      4
#define FORLOOP_STATE_PARENT    5
#define FORLOOP_STATE_SIZE      6

/* Cached ForloopDrop class and related methods */
static VALUE cLiquidForloopDrop = Qnil;
static VALUE str_forloop = Qnil;  /* "forloop" string for scope key */
static ID id_new;
static ID id_send;
static ID id_increment_bang;
static ID id_to_a;

/* Create a new forloop drop object */
static VALUE create_forloop_drop(long length, VALUE name, VALUE parent_forloop)
{
    if (cLiquidForloopDrop == Qnil) {
        /* Fallback: try to get the class at runtime */
        if (rb_const_defined(mLiquid, rb_intern("ForloopDrop"))) {
            cLiquidForloopDrop = rb_const_get(mLiquid, rb_intern("ForloopDrop"));
        } else {
            /* No ForloopDrop available, return nil */
            return Qnil;
        }
    }

    /* ForloopDrop.new(name, length, parentloop) */
    return rb_funcall(cLiquidForloopDrop, id_new, 3, name, LONG2NUM(length), parent_forloop);
}

static VALUE vm_internal_new(VALUE context)
{
    vm_t *vm;
    VALUE obj = TypedData_Make_Struct(cLiquidCVM, vm_t, &vm_data_type, vm);
    vm->stack = c_buffer_init();

    vm->invoking_filter = false;

    context_internal_init(context, &vm->context);

    return obj;
}

vm_t *vm_from_context(VALUE context)
{
    VALUE vm_obj = rb_attr_get(context, id_vm);
    if (vm_obj == Qnil) {
        vm_obj = vm_internal_new(context);
        rb_ivar_set(context, id_vm, vm_obj);
    }
    // instance variable is hidden from ruby so should be safe to unwrap it without type checking
    return DATA_PTR(vm_obj);
}

bool liquid_vm_filtering(VALUE context)
{
    VALUE vm_obj = rb_attr_get(context, id_vm);
    if (vm_obj == Qnil)
        return false;
    vm_t *vm = DATA_PTR(vm_obj);
    return vm->invoking_filter;
}

static void write_fixnum(VALUE output, VALUE fixnum)
{
    long long number = RB_NUM2LL(fixnum);
    int write_length = snprintf(NULL, 0, "%lld", number);
    long old_size = RSTRING_LEN(output);
    long new_size = old_size + write_length;
    long capacity = rb_str_capacity(output);

    if (new_size > capacity) {
        capacity *= 2;
        if (new_size > capacity) {
            capacity = new_size;
        }
        rb_str_resize(output, capacity);
    }
    rb_str_set_len(output, new_size);

    snprintf(RSTRING_PTR(output) + old_size, write_length + 1, "%lld", number);
}

static VALUE obj_to_s(VALUE obj)
{
    VALUE str = rb_funcall(obj, id_to_s, 0);

    if (RB_LIKELY(RB_TYPE_P(str, T_STRING)))
        return str;

    rb_raise(rb_eTypeError, "%"PRIsVALUE"#to_s returned a non-String convertible value of type %"PRIsVALUE,
            rb_obj_class(obj), rb_obj_class(str));
}

static void write_obj(VALUE output, VALUE obj)
{
    switch (TYPE(obj)) {
        default:
            obj = obj_to_s(obj);
            // fallthrough
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
    }
}

static inline void vm_stack_push(vm_t *vm, VALUE value)
{
    VALUE *stack_ptr = (VALUE *)vm->stack.data_end;
    assert(stack_ptr < (VALUE *)vm->stack.capacity_end);
    *stack_ptr++ = value;
    vm->stack.data_end = (uint8_t *)stack_ptr;
}

static inline VALUE *vm_stack_peek_n(vm_t *vm, size_t n)
{
    VALUE *stack_ptr = (VALUE *)vm->stack.data_end;
    stack_ptr -= n;
    assert((VALUE *)vm->stack.data <= stack_ptr);
    return stack_ptr;
}

static inline VALUE *vm_stack_pop_n(vm_t *vm, size_t n)
{
    VALUE *stack_ptr = vm_stack_peek_n(vm, n);
    vm->stack.data_end = (uint8_t *)stack_ptr;
    return stack_ptr;
}

static inline VALUE vm_stack_pop(vm_t *vm)
{
    return *vm_stack_pop_n(vm, 1);
}

static inline void vm_stack_reserve_for_write(vm_t *vm, size_t num_values)
{
    c_buffer_reserve_for_write(&vm->stack, num_values * sizeof(VALUE));
}

static VALUE vm_invoke_filter(vm_t *vm, VALUE filter_name, size_t num_args)
{
    VALUE *popped_args = vm_stack_pop_n(vm, num_args);
    /* We have to copy popped_args_ptr to the stack because the VM
     * no longer holds onto these objects, so they have to exist on
     * the stack to ensure they don't get garbage collected. */
    VALUE *args = alloca(sizeof(VALUE *) * num_args);
    memcpy(args, popped_args, sizeof(VALUE *) * num_args);

    bool not_invokable = rb_hash_lookup(vm->context.filter_methods, filter_name) != Qtrue;
    if (RB_UNLIKELY(not_invokable)) {
        if (vm->context.strict_filters) {
            VALUE error_class = rb_const_get(mLiquid, rb_intern("UndefinedFilter"));
            rb_raise(error_class, "undefined filter %"PRIsVALUE, rb_sym2str(filter_name));
        }
        return args[0];
    }

    vm->invoking_filter = true;
    VALUE result = rb_funcallv(vm->context.strainer, RB_SYM2ID(filter_name), (int)num_args, args);
    vm->invoking_filter = false;
    return rb_funcall(result, id_to_liquid, 0);
}

typedef struct vm_render_until_error_args {
    vm_t *vm;
    const uint8_t *ip; // use for initial address and to save an address for rescuing
    const size_t *const_ptr;

    /* rendering fields */
    VALUE output;
    const uint8_t *node_line_number;
} vm_render_until_error_args_t;

static VALUE raise_invalid_integer(VALUE unused_arg, VALUE exc)
{
    rb_raise(cLiquidArgumentError, "invalid integer");
}

// Equivalent to Integer(string) if string is an instance of String
static VALUE try_string_to_integer(VALUE string)
{
    return rb_str_to_inum(string, 0, true);
}

static VALUE range_value_to_integer(VALUE value)
{
    if (RB_INTEGER_TYPE_P(value)) {
        return value;
    } else if (value == Qnil) {
        return INT2FIX(0);
    } else if (RB_TYPE_P(value, T_STRING)) {
        return rb_str_to_inum(value, 0, false); // equivalent to String#to_i
    } else {
        value = obj_to_s(value);
        return rb_rescue2(try_string_to_integer, value, raise_invalid_integer, Qnil, rb_eArgError, (VALUE)0);
    }
}

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
    const VALUE *constants = args->const_ptr;
    const uint8_t *ip = args->ip;
    vm_t *vm = args->vm;
    VALUE output = args->output;
    uint16_t constant_index;
    VALUE constant = Qnil;
    args->ip = NULL; // used by vm_render_rescue, NULL to indicate that it isn't in a rescue block

    while (true) {
        switch (*ip++) {
            case OP_LEAVE:
                return false;
            case OP_PUSH_NIL:
                vm_stack_push(vm, Qnil);
                break;
            case OP_PUSH_TRUE:
                vm_stack_push(vm, Qtrue);
                break;
            case OP_PUSH_FALSE:
                vm_stack_push(vm, Qfalse);
                break;
            case OP_PUSH_INT8:
            {
                int num = *(int8_t *)ip++; // signed
                vm_stack_push(vm, RB_INT2FIX(num));
                break;
            }
            case OP_PUSH_INT16:
            {
                int num = *(int8_t *)ip++; // big endian encoding, so first byte has sign
                num = (num << 8) | *ip++;
                vm_stack_push(vm, RB_INT2FIX(num));
                break;
            }
            case OP_FIND_STATIC_VAR:
            {
                constant_index = (ip[0] << 8) | ip[1];
                constant = constants[constant_index];
                ip += 2;
                VALUE value = context_find_variable(&vm->context, constant, Qtrue);
                vm_stack_push(vm, value);
                break;
            }
            case OP_FIND_VAR:
            {
                VALUE key = vm_stack_pop(vm);
                VALUE value = context_find_variable(&vm->context, key, Qtrue);
                vm_stack_push(vm, value);
                break;
            }
            case OP_LOOKUP_CONST_KEY:
            case OP_LOOKUP_COMMAND:
            {
                constant_index = (ip[0] << 8) | ip[1];
                constant = constants[constant_index];
                ip += 2;
                vm_stack_push(vm, constant);
            }
            /* fallthrough */
            case OP_LOOKUP_KEY:
            {
                bool is_command = ip[-3] == OP_LOOKUP_COMMAND;
                VALUE key = vm_stack_pop(vm);
                VALUE object = vm_stack_pop(vm);
                VALUE result = variable_lookup_key(vm->context.self, object, key, is_command);
                vm_stack_push(vm, result);
                break;
            }

            case OP_NEW_INT_RANGE:
            {
                VALUE end = range_value_to_integer(vm_stack_pop(vm));
                VALUE begin = range_value_to_integer(vm_stack_pop(vm));
                bool exclude_end = false;
                vm_stack_push(vm, rb_range_new(begin, end, exclude_end));
                break;
            }
            case OP_HASH_NEW:
            {
                size_t hash_size = *ip++;
                size_t num_keys_and_values = hash_size * 2;
                VALUE hash = rb_hash_new();

                VALUE *args_ptr = vm_stack_peek_n(vm, num_keys_and_values);
                hash_bulk_insert(num_keys_and_values, args_ptr, hash);
                vm_stack_pop_n(vm, num_keys_and_values);

                vm_stack_push(vm, hash);
                break;
            }
            case OP_FILTER:
            case OP_BUILTIN_FILTER:
            {
                VALUE filter_name;
                unsigned long num_args;

                if (ip[-1] == OP_FILTER) {
                    constant_index = (ip[0] << 8) | ip[1];
                    constant = constants[constant_index];
                    filter_name = RARRAY_AREF(constant, 0);
                    num_args = FIX2ULONG(RARRAY_AREF(constant, 1));
                    ip += 2;
                } else {
                    assert(ip[-1] == OP_BUILTIN_FILTER);
                    filter_name = builtin_filters[*ip++].sym;
                    num_args = *ip++; // includes input argument
                }

                VALUE result = vm_invoke_filter(vm, filter_name, num_args);
                vm_stack_push(vm, result);
                break;
            }

            // Rendering instructions

            case OP_WRITE_RAW_W:
            case OP_WRITE_RAW:
            {
                const char *text;
                size_t size;
                if (ip[-1] == OP_WRITE_RAW_W) {
                    size = bytes_to_uint24(ip);
                    text = (const char *)&ip[3];
                    ip += 3 + size;
                } else {
                    size = *ip;
                    text = (const char *)&ip[1];
                    ip += 1 + size;
                }
                rb_str_cat(output, text, size);
                resource_limits_increment_write_score(vm->context.resource_limits, output);
                break;
            }
            case OP_JUMP_FWD_W:
            {
                size_t size = bytes_to_uint24(ip);
                ip += 3 + size;
                break;
            }

            case OP_JUMP_FWD:
            {
                uint8_t size = *ip;
                ip += 1 + size;
                break;
            }

            case OP_PUSH_CONST:
            {
                constant_index = (ip[0] << 8) | ip[1];
                constant = constants[constant_index];
                ip += 2;
                vm_stack_push(vm, constant);
                break;
            }

            case OP_WRITE_NODE:
            {
                constant_index = (ip[0] << 8) | ip[1];
                constant = constants[constant_index];
                ip += 2;

                /* Optimize common tags by handling them natively instead of calling Ruby */
                VALUE node_class = rb_obj_class(constant);

                if (cLiquidIncrement != Qnil && node_class == cLiquidIncrement) {
                    /* Handle Increment tag natively */
                    VALUE var_name = rb_funcall(constant, id_variable_name, 0);
                    VALUE environments = vm->context.environments;
                    VALUE counters = Qnil;
                    if (RARRAY_LEN(environments) > 0) {
                        counters = RARRAY_AREF(environments, 0);
                    }
                    long val = 0;
                    if (counters != Qnil && RB_TYPE_P(counters, T_HASH)) {
                        VALUE current = rb_hash_aref(counters, var_name);
                        if (current != Qnil) {
                            val = NUM2LONG(current);
                        }
                        rb_hash_aset(counters, var_name, LONG2NUM(val + 1));
                    }
                    write_fixnum(output, LONG2NUM(val));
                } else if (cLiquidDecrement != Qnil && node_class == cLiquidDecrement) {
                    /* Handle Decrement tag natively */
                    VALUE var_name = rb_funcall(constant, id_variable_name, 0);
                    VALUE environments = vm->context.environments;
                    VALUE counters = Qnil;
                    if (RARRAY_LEN(environments) > 0) {
                        counters = RARRAY_AREF(environments, 0);
                    }
                    long val = 0;
                    if (counters != Qnil && RB_TYPE_P(counters, T_HASH)) {
                        VALUE current = rb_hash_aref(counters, var_name);
                        if (current != Qnil) {
                            val = NUM2LONG(current);
                        }
                        val--;
                        rb_hash_aset(counters, var_name, LONG2NUM(val));
                    }
                    write_fixnum(output, LONG2NUM(val));
                } else if (cLiquidComment != Qnil && node_class == cLiquidComment) {
                    /* Handle Comment tag natively - just do nothing */
                    /* Comment.render_to_output_buffer returns output unchanged */
                } else {
                    /* Default: call Ruby render_node */
                    rb_funcall(cLiquidBlockBody, id_render_node, 3, vm->context.self, output, constant);

                    if (RARRAY_LEN(vm->context.interrupts)) {
                        return false;
                    }
                }

                resource_limits_increment_write_score(vm->context.resource_limits, output);
                break;
            }
            case OP_RENDER_VARIABLE_RESCUE:
                // Save state used by vm_render_rescue to rescue from a variable rendering exception
                args->node_line_number = ip;
                // vm_render_rescue will iterate from this instruction to the instruction
                // following OP_POP_WRITE_VARIABLE to resume rendering from
                ip += 3;
                args->ip = ip;
                break;
            case OP_POP_WRITE:
            {
                VALUE var_result = vm_stack_pop(vm);
                if (vm->context.global_filter != Qnil)
                    var_result = rb_funcall(vm->context.global_filter, id_call, 1, var_result);
                write_obj(output, var_result);
                args->ip = NULL; // mark the end of a rescue block, used by vm_render_rescue
                resource_limits_increment_write_score(vm->context.resource_limits, output);
                break;
            }

            /* New control flow opcodes */
            case OP_JUMP:
            {
                int16_t offset = (int16_t)((ip[0] << 8) | ip[1]);
                ip += 2 + offset;
                break;
            }
            case OP_JUMP_W:
            {
                int32_t offset = (int32_t)((ip[0] << 16) | (ip[1] << 8) | ip[2]);
                /* Sign extend from 24-bit */
                if (offset & 0x800000) offset |= 0xFF000000;
                ip += 3 + offset;
                break;
            }
            case OP_JUMP_IF_FALSE:
            {
                VALUE cond = unwrap_drop_value(vm_stack_pop(vm));
                int16_t offset = (int16_t)((ip[0] << 8) | ip[1]);
                ip += 2;
                /* Liquid truthiness: only nil and false are falsy */
                if (cond == Qnil || cond == Qfalse) {
                    ip += offset;
                }
                break;
            }
            case OP_JUMP_IF_FALSE_W:
            {
                VALUE cond = unwrap_drop_value(vm_stack_pop(vm));
                int32_t offset = (int32_t)((ip[0] << 16) | (ip[1] << 8) | ip[2]);
                if (offset & 0x800000) offset |= 0xFF000000;
                ip += 3;
                if (cond == Qnil || cond == Qfalse) {
                    ip += offset;
                }
                break;
            }
            case OP_JUMP_IF_TRUE:
            {
                VALUE cond = unwrap_drop_value(vm_stack_pop(vm));
                int16_t offset = (int16_t)((ip[0] << 8) | ip[1]);
                ip += 2;
                /* Liquid truthiness: only nil and false are falsy */
                if (cond != Qnil && cond != Qfalse) {
                    ip += offset;
                }
                break;
            }
            case OP_JUMP_IF_TRUE_W:
            {
                VALUE cond = unwrap_drop_value(vm_stack_pop(vm));
                int32_t offset = (int32_t)((ip[0] << 16) | (ip[1] << 8) | ip[2]);
                if (offset & 0x800000) offset |= 0xFF000000;
                ip += 3;
                if (cond != Qnil && cond != Qfalse) {
                    ip += offset;
                }
                break;
            }

            /* Comparison operators */
            case OP_CMP_EQ:
            {
                VALUE b = vm_stack_pop(vm);
                VALUE a = vm_stack_pop(vm);
                VALUE result = vm_equal_variables(a, b);
                vm_stack_push(vm, (result != Qnil && result != Qfalse) ? Qtrue : Qfalse);
                break;
            }
            case OP_CMP_NE:
            {
                VALUE b = vm_stack_pop(vm);
                VALUE a = vm_stack_pop(vm);
                VALUE result = vm_equal_variables(a, b);
                vm_stack_push(vm, (result != Qnil && result != Qfalse) ? Qfalse : Qtrue);
                break;
            }
            case OP_CMP_LT:
            {
                VALUE b = unwrap_drop_value(vm_stack_pop(vm));
                VALUE a = unwrap_drop_value(vm_stack_pop(vm));
                /* Ordering comparisons with nil return false (not an error) */
                if (a == Qnil || b == Qnil) {
                    vm_stack_push(vm, Qfalse);
                } else {
                    VALUE cmp_result = rb_funcall(a, rb_intern("<=>"), 1, b);
                    if (cmp_result == Qnil) {
                        vm_stack_push(vm, Qfalse);
                    } else {
                        int cmp = rb_cmpint(cmp_result, a, b);
                        vm_stack_push(vm, cmp < 0 ? Qtrue : Qfalse);
                    }
                }
                break;
            }
            case OP_CMP_GT:
            {
                VALUE b = unwrap_drop_value(vm_stack_pop(vm));
                VALUE a = unwrap_drop_value(vm_stack_pop(vm));
                /* Ordering comparisons with nil return false (not an error) */
                if (a == Qnil || b == Qnil) {
                    vm_stack_push(vm, Qfalse);
                } else {
                    VALUE cmp_result = rb_funcall(a, rb_intern("<=>"), 1, b);
                    if (cmp_result == Qnil) {
                        vm_stack_push(vm, Qfalse);
                    } else {
                        int cmp = rb_cmpint(cmp_result, a, b);
                        vm_stack_push(vm, cmp > 0 ? Qtrue : Qfalse);
                    }
                }
                break;
            }
            case OP_CMP_LE:
            {
                VALUE b = unwrap_drop_value(vm_stack_pop(vm));
                VALUE a = unwrap_drop_value(vm_stack_pop(vm));
                /* Ordering comparisons with nil return false (not an error) */
                if (a == Qnil || b == Qnil) {
                    vm_stack_push(vm, Qfalse);
                } else {
                    VALUE cmp_result = rb_funcall(a, rb_intern("<=>"), 1, b);
                    if (cmp_result == Qnil) {
                        vm_stack_push(vm, Qfalse);
                    } else {
                        int cmp = rb_cmpint(cmp_result, a, b);
                        vm_stack_push(vm, cmp <= 0 ? Qtrue : Qfalse);
                    }
                }
                break;
            }
            case OP_CMP_GE:
            {
                VALUE b = unwrap_drop_value(vm_stack_pop(vm));
                VALUE a = unwrap_drop_value(vm_stack_pop(vm));
                /* Ordering comparisons with nil return false (not an error) */
                if (a == Qnil || b == Qnil) {
                    vm_stack_push(vm, Qfalse);
                } else {
                    VALUE cmp_result = rb_funcall(a, rb_intern("<=>"), 1, b);
                    if (cmp_result == Qnil) {
                        vm_stack_push(vm, Qfalse);
                    } else {
                        int cmp = rb_cmpint(cmp_result, a, b);
                        vm_stack_push(vm, cmp >= 0 ? Qtrue : Qfalse);
                    }
                }
                break;
            }
            case OP_CMP_CONTAINS:
            {
                VALUE b = vm_stack_pop(vm);
                VALUE a = vm_stack_pop(vm);
                VALUE result = Qfalse;
                /* nil is not a valid operand for contains - always return false */
                if (b != Qnil) {
                    if (RB_TYPE_P(a, T_STRING) && RB_TYPE_P(b, T_STRING)) {
                        result = rb_funcall(a, rb_intern("include?"), 1, b);
                    } else if (RB_TYPE_P(a, T_ARRAY)) {
                        result = rb_funcall(a, rb_intern("include?"), 1, b);
                    } else if (RB_TYPE_P(a, T_HASH)) {
                        result = rb_funcall(a, rb_intern("key?"), 1, b);
                    }
                }
                vm_stack_push(vm, RTEST(result) ? Qtrue : Qfalse);
                break;
            }

            /* Logical operators */
            case OP_NOT:
            {
                VALUE val = unwrap_drop_value(vm_stack_pop(vm));
                /* Liquid truthiness: only nil and false are falsy */
                vm_stack_push(vm, (val == Qnil || val == Qfalse) ? Qtrue : Qfalse);
                break;
            }
            case OP_TRUTHY:
            {
                VALUE val = unwrap_drop_value(vm_stack_pop(vm));
                vm_stack_push(vm, (val != Qnil && val != Qfalse) ? Qtrue : Qfalse);
                break;
            }

            /* Variable assignment */
            case OP_ASSIGN:
            {
                constant_index = (ip[0] << 8) | ip[1];
                constant = constants[constant_index];
                ip += 2;
                VALUE value = vm_stack_pop(vm);
                /* Assign to the innermost scope */
                VALUE scopes = vm->context.scopes;
                if (RARRAY_LEN(scopes) > 0) {
                    VALUE scope = RARRAY_AREF(scopes, RARRAY_LEN(scopes) - 1);
                    rb_hash_aset(scope, constant, value);
                }
                break;
            }

            /* Counter operations */
            case OP_INCREMENT:
            {
                constant_index = (ip[0] << 8) | ip[1];
                constant = constants[constant_index];
                ip += 2;
                /* Get current value, default to 0 */
                VALUE environments = vm->context.environments;
                VALUE counters = Qnil;
                if (RARRAY_LEN(environments) > 0) {
                    counters = RARRAY_AREF(environments, 0);
                }
                long val = 0;
                if (counters != Qnil && RB_TYPE_P(counters, T_HASH)) {
                    VALUE current = rb_hash_aref(counters, constant);
                    if (current != Qnil) {
                        val = NUM2LONG(current);
                    }
                    rb_hash_aset(counters, constant, LONG2NUM(val + 1));
                }
                write_fixnum(output, LONG2NUM(val));
                resource_limits_increment_write_score(vm->context.resource_limits, output);
                break;
            }
            case OP_DECREMENT:
            {
                constant_index = (ip[0] << 8) | ip[1];
                constant = constants[constant_index];
                ip += 2;
                VALUE environments = vm->context.environments;
                VALUE counters = Qnil;
                if (RARRAY_LEN(environments) > 0) {
                    counters = RARRAY_AREF(environments, 0);
                }
                long val = 0;
                if (counters != Qnil && RB_TYPE_P(counters, T_HASH)) {
                    VALUE current = rb_hash_aref(counters, constant);
                    if (current != Qnil) {
                        val = NUM2LONG(current);
                    }
                    val--;
                    rb_hash_aset(counters, constant, LONG2NUM(val));
                }
                write_fixnum(output, LONG2NUM(val));
                resource_limits_increment_write_score(vm->context.resource_limits, output);
                break;
            }

            /* For loop opcodes */
            case OP_FOR_INIT:
            {
                /* Operands: uint16 var_name_idx, uint8 flags */
                constant_index = (ip[0] << 8) | ip[1];
                VALUE var_name = constants[constant_index];
                uint8_t flags = ip[2];
                ip += 3;

                /* Pop collection from stack */
                VALUE collection = vm_stack_pop(vm);

                /* Convert to array */
                VALUE items;
                if (RB_TYPE_P(collection, T_ARRAY)) {
                    items = collection;
                } else if (collection == Qnil) {
                    items = rb_ary_new();
                } else {
                    /* Call to_a on the collection */
                    items = rb_funcall(collection, id_to_a, 0);
                }

                /* Handle reversed flag */
                if (flags & 0x01) {  /* FOR_FLAG_REVERSED */
                    items = rb_ary_reverse(rb_ary_dup(items));
                }

                long length = RARRAY_LEN(items);

                /* Get current forloop (parent) from scope if it exists */
                VALUE parent_forloop = Qnil;
                VALUE scopes = vm->context.scopes;
                if (RARRAY_LEN(scopes) > 0) {
                    VALUE scope = RARRAY_AREF(scopes, RARRAY_LEN(scopes) - 1);
                    VALUE existing = rb_hash_aref(scope, str_forloop);
                    if (existing != Qnil) {
                        parent_forloop = existing;
                    }
                }

                /* Create ForloopDrop object */
                VALUE forloop_drop = create_forloop_drop(length, var_name, parent_forloop);

                /* Create iterator state array */
                VALUE state = rb_ary_new_capa(FORLOOP_STATE_SIZE);
                rb_ary_store(state, FORLOOP_STATE_ITEMS, items);
                rb_ary_store(state, FORLOOP_STATE_INDEX, LONG2NUM(-1));  /* Start at -1, FOR_NEXT increments to 0 */
                rb_ary_store(state, FORLOOP_STATE_LENGTH, LONG2NUM(length));
                rb_ary_store(state, FORLOOP_STATE_VAR_NAME, var_name);
                rb_ary_store(state, FORLOOP_STATE_DROP, forloop_drop);
                rb_ary_store(state, FORLOOP_STATE_PARENT, parent_forloop);

                /* Push forloop to current scope */
                if (RARRAY_LEN(scopes) > 0) {
                    VALUE scope = RARRAY_AREF(scopes, RARRAY_LEN(scopes) - 1);
                    if (forloop_drop != Qnil) {
                        rb_hash_aset(scope, str_forloop, forloop_drop);
                    }
                }

                /* Push state onto stack */
                vm_stack_push(vm, state);
                break;
            }

            case OP_FOR_NEXT:
            {
                /* Operands: int16 done_offset (where to jump if iteration complete) */
                int16_t done_offset = (int16_t)((ip[0] << 8) | ip[1]);
                ip += 2;

                /* Peek at iterator state (don't pop - we need it for the loop body) */
                VALUE state = *vm_stack_peek_n(vm, 1);

                VALUE items = RARRAY_AREF(state, FORLOOP_STATE_ITEMS);
                long index = NUM2LONG(RARRAY_AREF(state, FORLOOP_STATE_INDEX));
                long length = NUM2LONG(RARRAY_AREF(state, FORLOOP_STATE_LENGTH));
                VALUE var_name = RARRAY_AREF(state, FORLOOP_STATE_VAR_NAME);
                VALUE forloop_drop = RARRAY_AREF(state, FORLOOP_STATE_DROP);

                /* Increment index */
                index++;
                rb_ary_store(state, FORLOOP_STATE_INDEX, LONG2NUM(index));

                /* Check if we're done */
                if (index >= length) {
                    /* Jump to done offset */
                    ip += done_offset;
                } else {
                    /* Get current item and assign to loop variable */
                    VALUE item = RARRAY_AREF(items, index);

                    /* Assign item to loop variable in scope */
                    VALUE scopes = vm->context.scopes;
                    if (RARRAY_LEN(scopes) > 0) {
                        VALUE scope = RARRAY_AREF(scopes, RARRAY_LEN(scopes) - 1);
                        rb_hash_aset(scope, var_name, item);
                    }

                    /* Update forloop drop (increment! advances internal state).
                     * ForloopDrop starts with correct state for first item (index=1, first=true),
                     * so we only call increment! after the first iteration (index > 0). */
                    if (forloop_drop != Qnil && index > 0) {
                        rb_funcall(forloop_drop, id_increment_bang, 0);
                    }
                }
                break;
            }

            case OP_FOR_CLEANUP:
            {
                /* No operands */
                /* Pop iterator state from stack */
                VALUE state = vm_stack_pop(vm);

                /* Restore parent forloop in scope */
                VALUE parent_forloop = RARRAY_AREF(state, FORLOOP_STATE_PARENT);
                VALUE var_name = RARRAY_AREF(state, FORLOOP_STATE_VAR_NAME);

                VALUE scopes = vm->context.scopes;
                if (RARRAY_LEN(scopes) > 0) {
                    VALUE scope = RARRAY_AREF(scopes, RARRAY_LEN(scopes) - 1);
                    if (parent_forloop != Qnil) {
                        rb_hash_aset(scope, str_forloop, parent_forloop);
                    } else {
                        rb_hash_delete(scope, str_forloop);
                    }
                    /* Remove loop variable from scope */
                    rb_hash_delete(scope, var_name);
                }
                break;
            }

            case OP_DUP:
            {
                /* Duplicate top of stack */
                VALUE *top = vm_stack_peek_n(vm, 1);
                vm_stack_push(vm, *top);
                break;
            }

            case OP_POP_DISCARD:
            {
                /* Pop and discard top of stack */
                vm_stack_pop(vm);
                break;
            }

            default:
                rb_bug("invalid opcode: %u", ip[-1]);
        }
    }
}

typedef struct vm_evaluate_rescue_args {
    vm_render_until_error_args_t *render_args;
    size_t old_stack_byte_size;
} vm_evaluate_rescue_args_t;

static VALUE vm_evaluate_rescue(VALUE uncast_args, VALUE exception)
{
    vm_evaluate_rescue_args_t *args = (void *)uncast_args;
    vm_render_until_error_args_t *render_args = args->render_args;
    vm_t *vm = render_args->vm;

    vm->stack.data_end = vm->stack.data + args->old_stack_byte_size;

    rb_exc_raise(exception);
    return Qnil;
}

// Evaluate instructions that avoid using rendering instructions and leave with the result on
// the top of the stack
VALUE liquid_vm_evaluate(VALUE context, vm_assembler_t *code)
{
    vm_t *vm = vm_from_context(context);
    vm_stack_reserve_for_write(vm, code->max_stack_size);

    vm_render_until_error_args_t args = {
        .vm = vm,
        .const_ptr = (const size_t *)code->constants.data,
        .ip = code->instructions.data
    };
    vm_evaluate_rescue_args_t rescue_args = {
        .render_args = &args,
        .old_stack_byte_size = c_buffer_size(&vm->stack),
    };
    rb_rescue(vm_render_until_error, (VALUE)&args, vm_evaluate_rescue, (VALUE)&rescue_args);

    VALUE ret = vm_stack_pop(vm);
    assert(rescue_args.old_stack_byte_size == c_buffer_size(&vm->stack));
    return ret;
}

void liquid_vm_next_instruction(const uint8_t **ip_ptr)
{
    const uint8_t *ip = *ip_ptr;

    switch (*ip++) {
        case OP_LEAVE:
        case OP_POP_WRITE:
        case OP_PUSH_NIL:
        case OP_PUSH_TRUE:
        case OP_PUSH_FALSE:
        case OP_FIND_VAR:
        case OP_LOOKUP_KEY:
        case OP_NEW_INT_RANGE:
        /* New no-operand opcodes */
        case OP_CMP_EQ:
        case OP_CMP_NE:
        case OP_CMP_LT:
        case OP_CMP_GT:
        case OP_CMP_LE:
        case OP_CMP_GE:
        case OP_CMP_CONTAINS:
        case OP_NOT:
        case OP_TRUTHY:
        case OP_FOR_CLEANUP:
        case OP_CAPTURE_START:
        case OP_TABLEROW_COL_START:
        case OP_TABLEROW_COL_END:
        case OP_TABLEROW_CLEANUP:
        case OP_DUP:
        case OP_POP_DISCARD:
            break;

        case OP_HASH_NEW:
        case OP_PUSH_INT8:
            ip++;
            break;

        case OP_BUILTIN_FILTER:
        case OP_PUSH_INT16:
        case OP_PUSH_CONST:
        case OP_WRITE_NODE:
        case OP_FIND_STATIC_VAR:
        case OP_LOOKUP_CONST_KEY:
        case OP_LOOKUP_COMMAND:
        case OP_FILTER:
        /* New 2-byte operand opcodes */
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_TRUE:
        case OP_FOR_NEXT:
        case OP_TABLEROW_NEXT:
        case OP_ASSIGN:
        case OP_CAPTURE_END:
        case OP_INCREMENT:
        case OP_DECREMENT:
            ip += 2;
            break;

        case OP_RENDER_VARIABLE_RESCUE:
        /* New 3-byte operand opcodes */
        case OP_JUMP_W:
        case OP_JUMP_IF_FALSE_W:
        case OP_JUMP_IF_TRUE_W:
        case OP_FOR_INIT:
        case OP_TABLEROW_INIT:
        case OP_CYCLE:
            ip += 3;
            break;

        case OP_WRITE_RAW_W:
        case OP_JUMP_FWD_W:
        {
            size_t size = bytes_to_uint24(ip);
            ip += 3 + size;
            break;
        }

        case OP_WRITE_RAW:
        case OP_JUMP_FWD:
        {
            uint8_t size = *ip;
            ip += 1 + size;
            break;
        }

        default:
            rb_bug("invalid opcode: %u", ip[-1]);
    }
    *ip_ptr = ip;
}

VALUE vm_translate_if_filter_argument_error(vm_t *vm, VALUE exception)
{
    if (vm->invoking_filter) {
        if (rb_obj_is_kind_of(exception, rb_eArgError)) {
            VALUE cLiquidStrainerTemplate = rb_const_get(mLiquid, rb_intern("StrainerTemplate"));
            exception = rb_funcall(cLiquidStrainerTemplate, rb_intern("arg_exc_to_liquid_exc"), 1, exception);
        }
        vm->invoking_filter = false;
    }
    return exception;
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
    vm_t *vm = render_args->vm;

    exception = vm_translate_if_filter_argument_error(vm, exception);

    const uint8_t *ip = render_args->ip;
    if (!ip)
        rb_exc_raise(exception);

    // rescue for variable render, where ip is at the start of the render and we need to
    // skip to the end of the variable render to resume rendering if the error is handled
    enum opcode last_op;
    do {
        last_op = *ip;
        liquid_vm_next_instruction(&ip);
    } while (last_op != OP_POP_WRITE);
    render_args->ip = ip;
    // remove temporary stack values from variable evaluation
    vm->stack.data_end = vm->stack.data + args->old_stack_byte_size;

    assert(render_args->node_line_number);
    unsigned int node_line_number = bytes_to_uint24(render_args->node_line_number);
    VALUE line_number = node_line_number != 0 ? UINT2NUM(node_line_number) : Qnil;

    rb_funcall(cLiquidBlockBody, rb_intern("c_rescue_render_node"), 5,
        vm->context.self, render_args->output, line_number, exception, blank_tag);
    return true;
}

void liquid_vm_render(block_body_header_t *body, const VALUE *const_ptr, VALUE context, VALUE output)
{
    vm_t *vm = vm_from_context(context);

    vm_stack_reserve_for_write(vm, body->max_stack_size);
    resource_limits_increment_render_score(vm->context.resource_limits, body->render_score);

    vm_render_until_error_args_t render_args = {
        .vm = vm,
        .const_ptr = const_ptr,
        .ip = block_body_instructions_ptr(body),
        .output = output,
    };
    vm_render_rescue_args_t rescue_args = {
        .render_args = &render_args,
        .old_stack_byte_size = c_buffer_size(&vm->stack),
    };

    while (rb_rescue(vm_render_until_error, (VALUE)&render_args, vm_render_rescue, (VALUE)&rescue_args)) {
    }
    assert(rescue_args.old_stack_byte_size == c_buffer_size(&vm->stack));
}


void liquid_define_vm(void)
{
    id_render_node = rb_intern("render_node");
    id_vm = rb_intern("vm");
    id_variable_name = rb_intern("variable_name");
    id_to_liquid_value = rb_intern("to_liquid_value");

    /* For loop support */
    id_new = rb_intern("new");
    id_send = rb_intern("send");
    id_increment_bang = rb_intern("increment!");
    id_to_a = rb_intern("to_a");

    /* Initialize the "forloop" string for scope key lookups */
    str_forloop = rb_str_new_cstr("forloop");
    rb_str_freeze(str_forloop);
    rb_global_variable(&str_forloop);

    cLiquidCVM = rb_define_class_under(mLiquidC, "VM", rb_cObject);
    rb_undef_alloc_func(cLiquidCVM);
    rb_global_variable(&cLiquidCVM);

    /* Get Liquid::C::Empty::INSTANCE for empty keyword comparisons */
    VALUE cLiquidCEmpty = rb_const_get(mLiquidC, rb_intern("Empty"));
    empty_singleton = rb_const_get(cLiquidCEmpty, rb_intern("INSTANCE"));
    rb_global_variable(&empty_singleton);

    /* Get Liquid::C::Blank::INSTANCE for blank keyword comparisons */
    VALUE cLiquidCBlank = rb_const_get(mLiquidC, rb_intern("Blank"));
    blank_singleton = rb_const_get(cLiquidCBlank, rb_intern("INSTANCE"));
    rb_global_variable(&blank_singleton);

    /* Cache ForloopDrop class for native for loops */
    if (rb_const_defined(mLiquid, rb_intern("ForloopDrop"))) {
        cLiquidForloopDrop = rb_const_get(mLiquid, rb_intern("ForloopDrop"));
        rb_global_variable(&cLiquidForloopDrop);
    }

    /* Cache tag classes for native optimization.
     * These are looked up at runtime because they may not exist
     * when the extension is loaded. */
    if (rb_const_defined(mLiquid, rb_intern("Increment"))) {
        cLiquidIncrement = rb_const_get(mLiquid, rb_intern("Increment"));
        rb_global_variable(&cLiquidIncrement);
    }
    if (rb_const_defined(mLiquid, rb_intern("Decrement"))) {
        cLiquidDecrement = rb_const_get(mLiquid, rb_intern("Decrement"));
        rb_global_variable(&cLiquidDecrement);
    }
    if (rb_const_defined(mLiquid, rb_intern("Comment"))) {
        cLiquidComment = rb_const_get(mLiquid, rb_intern("Comment"));
        rb_global_variable(&cLiquidComment);
    }
}
