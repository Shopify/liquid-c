#include "vm_assembler.h"

void vm_assembler_init(vm_assembler_t *code)
{
    code->instructions = c_buffer_allocate(8);
    code->constants = c_buffer_allocate(8 * sizeof(VALUE));
    code->max_stack_size = 0;
    code->stack_size = 0;
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
                break;

            case OP_HASH_NEW:
                ip++;
                break;

            case OP_RENDER_VARIABLE_RESCUE:
                const_ptr++;
                break;

            case OP_WRITE_RAW:
                const_ptr += 2;
                break;

            case OP_WRITE_NODE:
            case OP_PUSH_CONST:
            case OP_PUSH_EVAL_EXPR:
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
