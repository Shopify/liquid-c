#include <stdint.h>

#include "liquid.h"
#include "vm.h"

ID id_render_node;
ID id_ivar_interrupts;
ID id_ivar_resource_limits;
ID id_increment_render_score;
ID id_increment_write_score;
ID id_raise_limits_reached;

void liquid_vm_render(block_body_t *body, VALUE context, VALUE output)
{
    VALUE resource_limits = rb_ivar_get(context, id_ivar_resource_limits);
    resource_limits_t *resource_limits_struct;
    ResourceLimits_Get_Struct(resource_limits, resource_limits_struct);

    rb_funcall(resource_limits, id_increment_render_score, 1, INT2NUM(body->render_score));

    bool is_captured = resource_limits_struct->last_capture_length != Qnil;
    long render_length_limit = LONG_MAX;

    if (!is_captured) {
        VALUE render_length_limit_num = resource_limits_struct->render_length_limit;
        if (render_length_limit_num != Qnil)
            render_length_limit = NUM2LONG(render_length_limit_num);
    }

    const size_t *const_ptr = (const size_t *)body->constants.data;
    const uint8_t *ip = body->instructions.data;
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
        if (RB_UNLIKELY(is_captured)) {
            rb_funcall(resource_limits, id_increment_write_score, 1, output);
        } else if (RSTRING_LEN(output) > render_length_limit) {
            rb_funcall(resource_limits, id_raise_limits_reached, 0);
        }
    }
}


void init_liquid_vm()
{
    id_render_node = rb_intern("render_node");
    id_ivar_interrupts = rb_intern("@interrupts");
    id_ivar_resource_limits = rb_intern("@resource_limits");
    id_increment_render_score = rb_intern("increment_render_score");
    id_increment_write_score = rb_intern("increment_write_score");
    id_raise_limits_reached = rb_intern("raise_limits_reached");
}
