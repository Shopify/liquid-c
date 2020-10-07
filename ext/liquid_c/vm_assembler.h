#ifndef VM_ASSEMBLER_H
#define VM_ASSEMBLER_H

#include <assert.h>
#include "c_buffer.h"

enum opcode {
    OP_LEAVE = 0,
    OP_WRITE_RAW = 1,
    OP_WRITE_NODE = 2,
    OP_POP_WRITE_VARIABLE,
    OP_PUSH_CONST,
    OP_HASH_NEW, // rb_hash_new & rb_hash_bulk_insert
    OP_FILTER,
    OP_PUSH_EVAL_EXPR,
    OP_RENDER_VARIABLE_RESCUE, // setup state to rescue variable rendering
};

typedef struct vm_assembler {
    c_buffer_t instructions;
    c_buffer_t constants;
    size_t max_stack_size;
    size_t stack_size;
} vm_assembler_t;

void vm_assembler_init(vm_assembler_t *code);
void vm_assembler_free(vm_assembler_t *code);
void vm_assembler_gc_mark(vm_assembler_t *code);
void vm_assembler_add_write_raw(vm_assembler_t *code, const char *string, size_t size);
void vm_assembler_add_write_node(vm_assembler_t *code, VALUE node);

static inline size_t vm_assembler_alloc_memsize(const vm_assembler_t *code)
{
    return c_buffer_capacity(&code->instructions) + c_buffer_capacity(&code->constants);
}

static inline void vm_assembler_write_opcode(vm_assembler_t *code, enum opcode op)
{
    c_buffer_write_byte(&code->instructions, op);
}

static inline void vm_assembler_write_ruby_constant(vm_assembler_t *code, VALUE constant)
{
    c_buffer_write(&code->constants, &constant, sizeof(VALUE));
}

static inline void vm_assembler_increment_stack_size(vm_assembler_t *code, size_t amount)
{
    code->stack_size += amount;
    if (code->stack_size > code->max_stack_size)
        code->max_stack_size = code->stack_size;
}


static inline void vm_assembler_add_leave(vm_assembler_t *code)
{
    vm_assembler_write_opcode(code, OP_LEAVE);
}

static inline void vm_assembler_remove_leave(vm_assembler_t *code)
{
    code->instructions.data_end--;
    assert(*code->instructions.data_end == OP_LEAVE);
}

static inline void vm_assembler_add_pop_write_variable(vm_assembler_t *code)
{
    code->stack_size -= 1;
    vm_assembler_write_opcode(code, OP_POP_WRITE_VARIABLE);
}

static inline void vm_assembler_add_hash_new(vm_assembler_t *code, uint8_t hash_size)
{
    code->stack_size -= hash_size * 2;
    code->stack_size++;
    uint8_t instructions[2] = { OP_HASH_NEW, hash_size };
    c_buffer_write(&code->instructions, &instructions, 2);
}

static inline void vm_assembler_add_push_const(vm_assembler_t *code, VALUE constant)
{
    vm_assembler_increment_stack_size(code, 1);
    vm_assembler_write_ruby_constant(code, constant);
    vm_assembler_write_opcode(code, OP_PUSH_CONST);
}

static inline void vm_assembler_add_push_eval_expr(vm_assembler_t *code, VALUE expression)
{
    vm_assembler_increment_stack_size(code, 1);
    vm_assembler_write_ruby_constant(code, expression);
    vm_assembler_write_opcode(code, OP_PUSH_EVAL_EXPR);
}

static inline void vm_assembler_add_filter(vm_assembler_t *code, VALUE filter_name, uint8_t arg_count)
{
    code->stack_size -= arg_count;
    vm_assembler_write_ruby_constant(code, filter_name);
    uint8_t instructions[2] = { OP_FILTER, arg_count + 1 /* include input */ };
    c_buffer_write(&code->instructions, &instructions, 2);
}

static inline void vm_assembler_add_render_variable_rescue(vm_assembler_t *code, size_t node_line_number)
{
    c_buffer_write(&code->constants, &node_line_number, sizeof(size_t));
    vm_assembler_write_opcode(code, OP_RENDER_VARIABLE_RESCUE);
}

#endif
