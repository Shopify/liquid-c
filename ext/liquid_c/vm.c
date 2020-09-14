#include <stdint.h>

#include "liquid.h"
#include "vm.h"
#include "resource_limits.h"

ID id_render_node;
ID id_ivar_interrupts;
ID id_ivar_resource_limits;
ID id_vm;

VALUE cLiquidCVM;

typedef struct vm {
    VALUE interrupts;
    VALUE resource_limits_obj;
    resource_limits_t *resource_limits;
} vm_t;

static void vm_mark(void *ptr)
{
    vm_t *vm = ptr;
    rb_gc_mark(vm->interrupts);
    rb_gc_mark(vm->resource_limits_obj);
}

static void vm_free(void *ptr)
{
    vm_t *vm = ptr;
    xfree(vm);
}

static size_t vm_memsize(const void *ptr)
{
    return sizeof(vm_t);
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

    vm->interrupts = rb_ivar_get(context, id_ivar_interrupts);
    Check_Type(vm->interrupts, T_ARRAY);

    vm->resource_limits_obj = rb_ivar_get(context, id_ivar_resource_limits);;
    ResourceLimits_Get_Struct(vm->resource_limits_obj, vm->resource_limits);
    return obj;
}

static vm_t *vm_from_context(VALUE context)
{
    VALUE vm_obj = rb_attr_get(context, id_vm);
    if (vm_obj == Qnil) {
        vm_obj = vm_internal_new(context);
        rb_ivar_set(context, id_vm, vm_obj);
    }
    // instance variable is hidden from C so should be safe to unwrap it without type checking
    return DATA_PTR(vm_obj);
}

void liquid_vm_next_instruction(const uint8_t **ip_ptr, const size_t **const_ptr_ptr)
{
    const uint8_t *ip = *ip_ptr;

    switch (*ip++) {
        case OP_LEAVE:
            break;

        case OP_WRITE_NODE:
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

void liquid_vm_render(block_body_t *body, VALUE context, VALUE output)
{
    vm_t *vm = vm_from_context(context);
    resource_limits_increment_render_score(vm->resource_limits, body->render_score);

    const size_t *const_ptr = (const size_t *)body->code.constants.data;
    const uint8_t *ip = body->code.instructions.data;
    VALUE interrupts = vm->interrupts;

    while (true) {
        switch (*ip++) {
            case OP_LEAVE:
                return;
            case OP_WRITE_RAW:
            {
                const char *text = (const char *)*const_ptr++;
                size_t size = *const_ptr++;
                rb_str_cat(output, text, size);
                break;
            }
            case OP_WRITE_NODE:
                rb_funcall(cLiquidBlockBody, id_render_node, 3, context, output, (VALUE)*const_ptr++);
                if (RARRAY_LEN(interrupts)) {
                    return;
                }
                break;
            default:
                rb_bug("invalid opcode: %u", ip[-1]);
        }

        resource_limits_increment_write_score(vm->resource_limits, output);
    }
}


void init_liquid_vm()
{
    id_render_node = rb_intern("render_node");
    id_ivar_interrupts = rb_intern("@interrupts");
    id_ivar_resource_limits = rb_intern("@resource_limits");
    id_vm = rb_intern("vm");

    cLiquidCVM = rb_define_class_under(mLiquidC, "VM", rb_cObject);
    rb_undef_alloc_func(cLiquidCVM);
    rb_global_variable(&cLiquidCVM);
}
