#ifndef VM_ASSEMBLER_H
#define VM_ASSEMBLER_H

#include <assert.h>
#include "liquid.h"
#include "c_buffer.h"

enum opcode {
    OP_LEAVE = 0,
    OP_WRITE_RAW = 1,
    OP_WRITE_NODE = 2,
    OP_POP_WRITE,
    OP_PUSH_CONST,
    OP_PUSH_NIL,
    OP_PUSH_TRUE,
    OP_PUSH_FALSE,
    OP_PUSH_INT8,
    OP_PUSH_INT16,
    OP_FIND_STATIC_VAR,
    OP_FIND_VAR,
    OP_LOOKUP_CONST_KEY,
    OP_LOOKUP_KEY,
    OP_LOOKUP_COMMAND,
    OP_NEW_INT_RANGE,
    OP_HASH_NEW, // rb_hash_new & rb_hash_bulk_insert
    OP_FILTER,
    OP_RENDER_VARIABLE_RESCUE, // setup state to rescue variable rendering
};

typedef struct vm_assembler {
    c_buffer_t instructions;
    c_buffer_t constants;
    size_t max_stack_size;
    size_t stack_size;
    bool parsing; // prevent executing when incomplete or extending when complete
} vm_assembler_t;

void init_liquid_vm_assembler();
void vm_assembler_init(vm_assembler_t *code);
void vm_assembler_free(vm_assembler_t *code);
void vm_assembler_gc_mark(vm_assembler_t *code);
void vm_assembler_concat(vm_assembler_t *dest, vm_assembler_t *src);

void vm_assembler_add_write_raw(vm_assembler_t *code, const char *string, size_t size);
void vm_assembler_add_write_node(vm_assembler_t *code, VALUE node);
void vm_assembler_add_push_fixnum(vm_assembler_t *code, VALUE num);
void vm_assembler_add_push_literal(vm_assembler_t *code, VALUE literal);

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

static inline void vm_assembler_reserve_stack_size(vm_assembler_t *code, size_t amount)
{
    vm_assembler_increment_stack_size(code, amount);
    code->stack_size -= amount;
}


static inline void vm_assembler_add_leave(vm_assembler_t *code)
{
    vm_assembler_write_opcode(code, OP_LEAVE);
    code->parsing = false;
}

static inline void vm_assembler_remove_leave(vm_assembler_t *code)
{
    code->parsing = true;
    code->instructions.data_end--;
    assert(*code->instructions.data_end == OP_LEAVE);
}

static inline void vm_assembler_add_pop_write(vm_assembler_t *code)
{
    code->stack_size -= 1;
    vm_assembler_write_opcode(code, OP_POP_WRITE);
}

static inline void vm_assembler_add_hash_new(vm_assembler_t *code, uint8_t hash_size)
{
    code->stack_size -= hash_size * 2;
    code->stack_size++;
    uint8_t instructions[2] = { OP_HASH_NEW, hash_size };
    c_buffer_write(&code->instructions, &instructions, 2);
}


static inline void vm_assembler_add_push_nil(vm_assembler_t *code)
{
    vm_assembler_increment_stack_size(code, 1);
    vm_assembler_write_opcode(code, OP_PUSH_NIL);
}

static inline void vm_assembler_add_push_true(vm_assembler_t *code)
{
    vm_assembler_increment_stack_size(code, 1);
    vm_assembler_write_opcode(code, OP_PUSH_TRUE);
}

static inline void vm_assembler_add_push_false(vm_assembler_t *code)
{
    vm_assembler_increment_stack_size(code, 1);
    vm_assembler_write_opcode(code, OP_PUSH_FALSE);
}

static inline void vm_assembler_add_push_int8(vm_assembler_t *code, int8_t value)
{
    vm_assembler_increment_stack_size(code, 1);
    uint8_t instructions[2] = { OP_PUSH_INT8, value };
    c_buffer_write(&code->instructions, &instructions, sizeof(instructions));
}

static inline void vm_assembler_add_push_int16(vm_assembler_t *code, int16_t value)
{
    vm_assembler_increment_stack_size(code, 1);
    uint8_t instructions[3] = { OP_PUSH_INT16, value >> 8, (uint8_t)value };
    c_buffer_write(&code->instructions, &instructions, sizeof(instructions));
}

static inline void vm_assembler_add_push_const(vm_assembler_t *code, VALUE constant)
{
    vm_assembler_increment_stack_size(code, 1);
    vm_assembler_write_ruby_constant(code, constant);
    vm_assembler_write_opcode(code, OP_PUSH_CONST);
}

static inline void vm_assembler_add_find_static_variable(vm_assembler_t *code, VALUE key)
{
    vm_assembler_increment_stack_size(code, 1);
    vm_assembler_write_ruby_constant(code, key);
    vm_assembler_write_opcode(code, OP_FIND_STATIC_VAR);
}

static inline void vm_assembler_add_find_variable(vm_assembler_t *code)
{
    // pop 1, push 1
    vm_assembler_write_opcode(code, OP_FIND_VAR);
}

static inline void vm_assembler_add_lookup_const_key(vm_assembler_t *code, VALUE key)
{
    vm_assembler_reserve_stack_size(code, 1); // push 1, pop 2, push 1
    vm_assembler_write_ruby_constant(code, key);
    vm_assembler_write_opcode(code, OP_LOOKUP_CONST_KEY);
}

static inline void vm_assembler_add_lookup_key(vm_assembler_t *code)
{
    code->stack_size--; // pop 2, push 1
    vm_assembler_write_opcode(code, OP_LOOKUP_KEY);
}

static inline void vm_assembler_add_lookup_command(vm_assembler_t *code, VALUE command)
{
    vm_assembler_reserve_stack_size(code, 1); // push 1, pop 2, push 1
    vm_assembler_write_ruby_constant(code, command);
    vm_assembler_write_opcode(code, OP_LOOKUP_COMMAND);
}

static inline void vm_assembler_add_new_int_range(vm_assembler_t *code)
{
    code->stack_size--; // pop 2, push 1
    vm_assembler_write_opcode(code, OP_NEW_INT_RANGE);
}

static inline void vm_assembler_add_filter(vm_assembler_t *code, VALUE filter_name, uint8_t arg_count)
{
    code->stack_size -= arg_count; // pop arg_count + 1, push 1
    vm_assembler_write_ruby_constant(code, filter_name);
    uint8_t instructions[2] = { OP_FILTER, arg_count + 1 /* include input */ };
    c_buffer_write(&code->instructions, &instructions, 2);
}

static inline void vm_assembler_add_render_variable_rescue(vm_assembler_t *code, size_t node_line_number)
{
    uint8_t instructions[4] = { OP_RENDER_VARIABLE_RESCUE, node_line_number >> 16, node_line_number >> 8, node_line_number };
    c_buffer_write(&code->instructions, &instructions, sizeof(instructions));
}

#endif
