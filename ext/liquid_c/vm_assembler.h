#ifndef VM_ASSEMBLER_H
#define VM_ASSEMBLER_H

#include "c_buffer.h"

enum opcode {
    OP_LEAVE = 0,
    OP_WRITE_RAW = 1,
    OP_WRITE_NODE = 2,
};

typedef struct vm_assembler {
    c_buffer_t instructions;
    c_buffer_t constants;
} vm_assembler_t;

void vm_assembler_init(vm_assembler_t *code);
void vm_assembler_free(vm_assembler_t *code);
void vm_assembler_gc_mark(vm_assembler_t *code);
void vm_assembler_add_write_raw(vm_assembler_t *code, const char *string, size_t size);
void vm_assembler_add_write_node(vm_assembler_t *code, VALUE node);

static inline size_t vm_assembler_alloc_memsize(const vm_assembler_t *code)
{
    return code->instructions.capacity + code->constants.capacity;
}

static inline void vm_assembler_write_opcode(vm_assembler_t *code, enum opcode op)
{
    c_buffer_write_byte(&code->instructions, op);
}

static inline void vm_assembler_write_ruby_constant(vm_assembler_t *code, VALUE constant)
{
    c_buffer_write(&code->constants, &constant, sizeof(VALUE));
}

static inline void vm_assembler_add_leave(vm_assembler_t *code)
{
    vm_assembler_write_opcode(code, OP_LEAVE);
}

static inline void vm_assembler_remove_leave(vm_assembler_t *code)
{
    code->instructions.size -= 1;
}

#endif
