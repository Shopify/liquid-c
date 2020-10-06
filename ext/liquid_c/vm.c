#include <stdint.h>

#include "liquid.h"
#include "vm.h"
#include "resource_limits.h"

ID id_render_node;
ID id_ivar_interrupts;
ID id_ivar_resource_limits;

void liquid_vm_render(block_body_t *body, VALUE context, VALUE output)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(rb_ivar_get(context, id_ivar_resource_limits), resource_limits);

    resource_limits_increment_render_score(resource_limits, body->render_score);

    const size_t *const_ptr = (const size_t *)body->code.constants.data;
    const uint8_t *ip = body->code.instructions.data;
    VALUE interrupts = rb_ivar_get(context, id_ivar_interrupts);
    Check_Type(interrupts, T_ARRAY);

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

        resource_limits_increment_write_score(resource_limits, output);
    }
}


void init_liquid_vm()
{
    id_render_node = rb_intern("render_node");
    id_ivar_interrupts = rb_intern("@interrupts");
    id_ivar_resource_limits = rb_intern("@resource_limits");
}
