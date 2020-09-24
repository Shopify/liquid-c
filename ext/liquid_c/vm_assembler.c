#include "liquid.h"
#include "vm_assembler.h"

void vm_assembler_init(vm_assembler_t *code)
{
    code->instructions = c_buffer_allocate(8);
    code->constants = c_buffer_allocate(8 * sizeof(VALUE));
    code->max_stack_size = 0;
    code->stack_size = 0;
    code->parsing = true;
}

void vm_assembler_free(vm_assembler_t *code)
{
    c_buffer_free(&code->instructions);
    c_buffer_free(&code->constants);
}

void vm_assembler_gc_mark(vm_assembler_t *code)
{
    size_t *const_ptr = (size_t *)code->constants.data;
    const uint8_t *ip = code->instructions.data;
    // Don't rely on a terminating OP_LEAVE instruction
    // since this could be called in the middle of parsing
    const uint8_t *end_ip = code->instructions.data_end;
    while (ip < end_ip) {
        switch (*ip++) {
            case OP_LEAVE:
            case OP_POP_WRITE_VARIABLE:
            case OP_PUSH_NIL:
            case OP_PUSH_TRUE:
            case OP_PUSH_FALSE:
            case OP_FIND_VAR:
            case OP_LOOKUP_KEY:
            case OP_NEW_INT_RANGE:
                break;

            case OP_HASH_NEW:
            case OP_PUSH_INT8:
                ip++;
                break;

            case OP_PUSH_INT16:
                ip += 2;
                break;

            case OP_RENDER_VARIABLE_RESCUE:
                ip += 3;
                break;

            case OP_WRITE_RAW:
                const_ptr += 2;
                break;

            case OP_WRITE_NODE:
            case OP_PUSH_CONST:
            case OP_FIND_STATIC_VAR:
            case OP_LOOKUP_CONST_KEY:
            case OP_LOOKUP_COMMAND:
                rb_gc_mark(*const_ptr++);
                break;

            case OP_FILTER:
                ip++;
                rb_gc_mark(*const_ptr++);
                break;

            default:
                rb_bug("invalid opcode: %u", ip[-1]);
        }
    }
}

void vm_assembler_concat(vm_assembler_t *dest, vm_assembler_t *src)
{
    c_buffer_concat(&dest->instructions, &src->instructions);
    c_buffer_concat(&dest->constants, &src->constants);

    size_t max_src_stack_size = dest->stack_size + src->max_stack_size;
    if (max_src_stack_size > dest->max_stack_size)
        dest->max_stack_size = max_src_stack_size;

    dest->stack_size += src->stack_size;
}


void vm_assembler_add_write_raw(vm_assembler_t *code, const char *string, size_t size)
{
    vm_assembler_write_opcode(code, OP_WRITE_RAW);
    size_t slice[2] = { (size_t)string, size };
    c_buffer_write(&code->constants, &slice, sizeof(slice));
}

void vm_assembler_add_write_node(vm_assembler_t *code, VALUE node)
{
    vm_assembler_write_opcode(code, OP_WRITE_NODE);
    vm_assembler_write_ruby_constant(code, node);
}

void vm_assembler_add_push_fixnum(vm_assembler_t *code, VALUE num)
{
    long x = FIX2LONG(num);
    if (x >= INT8_MIN && x <= INT8_MAX) {
        vm_assembler_add_push_int8(code, x);
    } else if (x >= INT16_MIN && x <= INT16_MAX) {
        vm_assembler_add_push_int16(code, x);
    } else {
        vm_assembler_add_push_const(code, num);
    }
}

void vm_assembler_add_push_literal(vm_assembler_t *code, VALUE literal)
{
    switch (literal) {
    case Qnil:
        vm_assembler_add_push_nil(code);
        break;
    case Qtrue:
        vm_assembler_add_push_true(code);
        break;
    case Qfalse:
        vm_assembler_add_push_false(code);
        break;
    default:
        if (RB_FIXNUM_P(literal)) {
            vm_assembler_add_push_fixnum(code, literal);
        } else {
            vm_assembler_add_push_const(code, literal);
        }
        break;
    }
}
