#ifndef VM_ASSEMBLER_H
#define VM_ASSEMBLER_H

#include <assert.h>
#include "liquid.h"
#include "c_buffer.h"
#include "intutil.h"

enum opcode {
    OP_LEAVE = 0,
    OP_WRITE_RAW_W = 1,
    OP_WRITE_NODE = 2,
    OP_POP_WRITE,
    OP_WRITE_RAW_SKIP,
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
    OP_BUILTIN_FILTER,
    OP_RENDER_VARIABLE_RESCUE, // setup state to rescue variable rendering
    OP_WRITE_RAW,
    OP_JUMP_FWD_W,
    OP_JUMP_FWD,

    /* New control flow opcodes for template parser */
    OP_JUMP,              /* Unconditional jump: int16 offset */
    OP_JUMP_W,            /* Wide unconditional jump: int24 offset */
    OP_JUMP_IF_FALSE,     /* Jump if falsy (Liquid rules): int16 offset */
    OP_JUMP_IF_FALSE_W,   /* Wide conditional jump */
    OP_JUMP_IF_TRUE,      /* Jump if truthy: int16 offset */
    OP_JUMP_IF_TRUE_W,    /* Wide conditional jump */

    /* Comparison operators (pop 2, push bool) */
    OP_CMP_EQ,            /* == */
    OP_CMP_NE,            /* != */
    OP_CMP_LT,            /* < */
    OP_CMP_GT,            /* > */
    OP_CMP_LE,            /* <= */
    OP_CMP_GE,            /* >= */
    OP_CMP_CONTAINS,      /* contains */

    /* Logical operators */
    OP_NOT,               /* Logical not (Liquid truthiness) */
    OP_TRUTHY,            /* Convert to Liquid boolean */

    /* For loop support */
    OP_FOR_INIT,          /* Initialize forloop: uint16 var_idx, uint8 flags */
    OP_FOR_NEXT,          /* Get next or jump: int16 done_offset */
    OP_FOR_CLEANUP,       /* Cleanup forloop object */

    /* Variable assignment */
    OP_ASSIGN,            /* Assign to variable: uint16 var_idx */
    OP_CAPTURE_START,     /* Start capturing output */
    OP_CAPTURE_END,       /* End capture, assign to var: uint16 var_idx */

    /* Counter operations */
    OP_INCREMENT,         /* Increment and write: uint16 var_idx */
    OP_DECREMENT,         /* Decrement and write: uint16 var_idx */

    /* Cycle support */
    OP_CYCLE,             /* Cycle through values: uint16 group_idx, uint8 count */

    /* Tablerow support */
    OP_TABLEROW_INIT,     /* Initialize tablerow */
    OP_TABLEROW_NEXT,     /* Get next or jump */
    OP_TABLEROW_COL_START,/* Write <td> with class */
    OP_TABLEROW_COL_END,  /* Write </td>, maybe </tr><tr> */
    OP_TABLEROW_CLEANUP,  /* Write final </tr> if needed */

    /* Stack manipulation */
    OP_DUP,               /* Duplicate top of stack */
    OP_POP_DISCARD,       /* Pop and discard top of stack */
};

typedef struct {
    const char *name;
    VALUE sym;
} filter_desc_t;

extern filter_desc_t builtin_filters[];

typedef struct vm_assembler {
    c_buffer_t instructions;
    c_buffer_t constants;
    st_table *constants_table;
    size_t max_stack_size;
    size_t stack_size;
    size_t protected_stack_size;
    bool parsing; // prevent executing when incomplete or extending when complete
} vm_assembler_t;

void liquid_define_vm_assembler(void);
void vm_assembler_init(vm_assembler_t *code);
void vm_assembler_reset(vm_assembler_t *code);
void vm_assembler_free(vm_assembler_t *code);
void vm_assembler_gc_mark(vm_assembler_t *code);
VALUE vm_assembler_disassemble(const uint8_t *start_ip, const uint8_t *end_ip, const VALUE *constants);
void vm_assembler_concat(vm_assembler_t *dest, vm_assembler_t *src);
void vm_assembler_require_stack_args(vm_assembler_t *code, unsigned int count);

void vm_assembler_add_write_raw(vm_assembler_t *code, const char *string, size_t size);
void vm_assembler_add_write_node(vm_assembler_t *code, VALUE node);
void vm_assembler_add_push_fixnum(vm_assembler_t *code, VALUE num);
void vm_assembler_add_push_literal(vm_assembler_t *code, VALUE literal);
void vm_assembler_add_filter(vm_assembler_t *code, VALUE filter_name, size_t arg_count);

void vm_assembler_add_evaluate_expression_from_ruby(vm_assembler_t *code, VALUE code_obj, VALUE expression);
void vm_assembler_add_find_variable_from_ruby(vm_assembler_t *code, VALUE code_obj, VALUE expression);
void vm_assembler_add_lookup_command_from_ruby(vm_assembler_t *code, VALUE command);
void vm_assembler_add_lookup_key_from_ruby(vm_assembler_t *code, VALUE code_obj, VALUE expression);
void vm_assembler_add_new_int_range_from_ruby(vm_assembler_t *code);
void vm_assembler_add_hash_new_from_ruby(vm_assembler_t *code, VALUE hash_size_obj);
void vm_assembler_add_filter_from_ruby(vm_assembler_t *code, VALUE filter_name, VALUE arg_count_obj);

bool vm_assembler_opcode_has_constant(uint8_t ip);

static inline size_t vm_assembler_alloc_memsize(const vm_assembler_t *code)
{
    return c_buffer_capacity(&code->instructions) + c_buffer_capacity(&code->constants) + sizeof(st_table);
}

static inline void vm_assembler_write_opcode(vm_assembler_t *code, enum opcode op)
{
    c_buffer_write_byte(&code->instructions, op);
}

static inline uint16_t vm_assembler_write_ruby_constant(vm_assembler_t *code, VALUE constant)
{
    st_table *constants_table = code->constants_table;
    st_data_t index_value;

    if (st_lookup(constants_table, constant, &index_value)) {
        return (uint16_t)index_value;
    } else {
        uint16_t index = c_buffer_size(&code->constants) / sizeof(VALUE);
        st_insert(constants_table, constant, index);
        c_buffer_write(&code->constants, &constant, sizeof(VALUE));
        return index;
    }
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

static inline void vm_assembler_add_op_with_constant(vm_assembler_t *code, VALUE constant, uint8_t opcode)
{
    uint16_t index = vm_assembler_write_ruby_constant(code, constant);
    uint8_t *instructions = c_buffer_extend_for_write(&code->instructions, 3);
    instructions[0] = opcode;
    instructions[1] = index >> 8;
    instructions[2] = (uint8_t)index;
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

static inline void vm_assembler_add_hash_new(vm_assembler_t *code, size_t hash_size)
{
    if (hash_size > 255)
        rb_enc_raise(utf8_encoding, cLiquidSyntaxError, "Hash literal has too many keys");
    code->stack_size -= hash_size * 2;
    code->stack_size++;
    uint8_t *instructions = c_buffer_extend_for_write(&code->instructions, 2);
    instructions[0] = OP_HASH_NEW;
    instructions[1] = hash_size;
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
    uint8_t *instructions = c_buffer_extend_for_write(&code->instructions, 2);
    instructions[0] = OP_PUSH_INT8;
    instructions[1] = value;
}

static inline void vm_assembler_add_push_int16(vm_assembler_t *code, int16_t value)
{
    vm_assembler_increment_stack_size(code, 1);
    uint8_t *instructions = c_buffer_extend_for_write(&code->instructions, 3);
    instructions[0] = OP_PUSH_INT16;
    instructions[1] = value >> 8;
    instructions[2] = (uint8_t)value;
}

static inline void vm_assembler_add_push_const(vm_assembler_t *code, VALUE constant)
{
    vm_assembler_increment_stack_size(code, 1);
    vm_assembler_add_op_with_constant(code, constant, OP_PUSH_CONST);
}

static inline void vm_assembler_add_find_static_variable(vm_assembler_t *code, VALUE key)
{
    vm_assembler_increment_stack_size(code, 1);
    vm_assembler_add_op_with_constant(code, key, OP_FIND_STATIC_VAR);
}

static inline void vm_assembler_add_find_variable(vm_assembler_t *code)
{
    // pop 1, push 1
    vm_assembler_write_opcode(code, OP_FIND_VAR);
}

static inline void vm_assembler_add_lookup_const_key(vm_assembler_t *code, VALUE key)
{
    vm_assembler_reserve_stack_size(code, 1); // push 1, pop 2, push 1
    vm_assembler_add_op_with_constant(code, key, OP_LOOKUP_CONST_KEY);
}

static inline void vm_assembler_add_lookup_key(vm_assembler_t *code)
{
    code->stack_size--; // pop 2, push 1
    vm_assembler_write_opcode(code, OP_LOOKUP_KEY);
}

static inline void vm_assembler_add_lookup_command(vm_assembler_t *code, VALUE command)
{
    vm_assembler_reserve_stack_size(code, 1); // push 1, pop 2, push 1
    vm_assembler_add_op_with_constant(code, command, OP_LOOKUP_COMMAND);
}

static inline void vm_assembler_add_new_int_range(vm_assembler_t *code)
{
    code->stack_size--; // pop 2, push 1
    vm_assembler_write_opcode(code, OP_NEW_INT_RANGE);
}

static inline void vm_assembler_add_render_variable_rescue(vm_assembler_t *code, size_t node_line_number)
{
    uint8_t *instructions = c_buffer_extend_for_write(&code->instructions, 4);
    instructions[0] = OP_RENDER_VARIABLE_RESCUE;
    uint24_to_bytes((unsigned int)node_line_number, &instructions[1]);
}

/* Get current instruction offset for jump target calculation */
static inline size_t vm_assembler_current_offset(vm_assembler_t *code)
{
    return c_buffer_size(&code->instructions);
}

/* Reserve space for a jump and return offset to patch later */
static inline size_t vm_assembler_add_jump_placeholder(vm_assembler_t *code, enum opcode op)
{
    size_t offset = vm_assembler_current_offset(code);
    uint8_t *instructions = c_buffer_extend_for_write(&code->instructions, 3);
    instructions[0] = op;
    instructions[1] = 0;
    instructions[2] = 0;
    return offset;
}

/* Reserve space for a wide jump and return offset to patch later */
static inline size_t vm_assembler_add_jump_placeholder_w(vm_assembler_t *code, enum opcode op)
{
    size_t offset = vm_assembler_current_offset(code);
    uint8_t *instructions = c_buffer_extend_for_write(&code->instructions, 4);
    instructions[0] = op;
    instructions[1] = 0;
    instructions[2] = 0;
    instructions[3] = 0;
    return offset;
}

/* Patch a jump instruction with the actual offset */
static inline void vm_assembler_patch_jump(vm_assembler_t *code, size_t jump_offset, size_t target_offset)
{
    uint8_t *instructions = code->instructions.data + jump_offset;
    int16_t relative = (int16_t)(target_offset - jump_offset - 3); /* 3 = opcode + 2 bytes offset */
    instructions[1] = (relative >> 8) & 0xFF;
    instructions[2] = relative & 0xFF;
}

/* Patch a wide jump instruction */
static inline void vm_assembler_patch_jump_w(vm_assembler_t *code, size_t jump_offset, size_t target_offset)
{
    uint8_t *instructions = code->instructions.data + jump_offset;
    int32_t relative = (int32_t)(target_offset - jump_offset - 4); /* 4 = opcode + 3 bytes offset */
    instructions[1] = (relative >> 16) & 0xFF;
    instructions[2] = (relative >> 8) & 0xFF;
    instructions[3] = relative & 0xFF;
}

/* Add unconditional jump (forward or backward) */
static inline void vm_assembler_add_jump(vm_assembler_t *code, int16_t offset)
{
    uint8_t *instructions = c_buffer_extend_for_write(&code->instructions, 3);
    instructions[0] = OP_JUMP;
    instructions[1] = (offset >> 8) & 0xFF;
    instructions[2] = offset & 0xFF;
}

/* Add conditional jump if top of stack is falsy */
static inline size_t vm_assembler_add_jump_if_false(vm_assembler_t *code)
{
    code->stack_size--; /* pops condition */
    return vm_assembler_add_jump_placeholder(code, OP_JUMP_IF_FALSE);
}

/* Add conditional jump if top of stack is truthy */
static inline size_t vm_assembler_add_jump_if_true(vm_assembler_t *code)
{
    code->stack_size--; /* pops condition */
    return vm_assembler_add_jump_placeholder(code, OP_JUMP_IF_TRUE);
}

/* Comparison operators - pop 2, push 1 */
static inline void vm_assembler_add_cmp_eq(vm_assembler_t *code)
{
    code->stack_size--; /* pop 2, push 1 */
    vm_assembler_write_opcode(code, OP_CMP_EQ);
}

static inline void vm_assembler_add_cmp_ne(vm_assembler_t *code)
{
    code->stack_size--;
    vm_assembler_write_opcode(code, OP_CMP_NE);
}

static inline void vm_assembler_add_cmp_lt(vm_assembler_t *code)
{
    code->stack_size--;
    vm_assembler_write_opcode(code, OP_CMP_LT);
}

static inline void vm_assembler_add_cmp_gt(vm_assembler_t *code)
{
    code->stack_size--;
    vm_assembler_write_opcode(code, OP_CMP_GT);
}

static inline void vm_assembler_add_cmp_le(vm_assembler_t *code)
{
    code->stack_size--;
    vm_assembler_write_opcode(code, OP_CMP_LE);
}

static inline void vm_assembler_add_cmp_ge(vm_assembler_t *code)
{
    code->stack_size--;
    vm_assembler_write_opcode(code, OP_CMP_GE);
}

static inline void vm_assembler_add_cmp_contains(vm_assembler_t *code)
{
    code->stack_size--;
    vm_assembler_write_opcode(code, OP_CMP_CONTAINS);
}

/* Logical operators */
static inline void vm_assembler_add_not(vm_assembler_t *code)
{
    /* pop 1, push 1 */
    vm_assembler_write_opcode(code, OP_NOT);
}

static inline void vm_assembler_add_truthy(vm_assembler_t *code)
{
    /* pop 1, push 1 */
    vm_assembler_write_opcode(code, OP_TRUTHY);
}

/* Variable assignment */
static inline void vm_assembler_add_assign(vm_assembler_t *code, VALUE var_name)
{
    code->stack_size--; /* pops value */
    vm_assembler_add_op_with_constant(code, var_name, OP_ASSIGN);
}

/* Increment counter and write */
static inline void vm_assembler_add_increment(vm_assembler_t *code, VALUE var_name)
{
    vm_assembler_add_op_with_constant(code, var_name, OP_INCREMENT);
}

/* Decrement counter and write */
static inline void vm_assembler_add_decrement(vm_assembler_t *code, VALUE var_name)
{
    vm_assembler_add_op_with_constant(code, var_name, OP_DECREMENT);
}

/* For loop opcodes */

/* Flags for FOR_INIT */
#define FOR_FLAG_REVERSED 0x01

/*
 * OP_FOR_INIT: Initialize for loop
 * Operands: uint16 var_name_idx, uint8 flags
 * Stack: [collection] -> [iterator_state]
 * - Creates forloop drop object
 * - Pushes iterator state (array + index) to stack
 * - Pushes forloop variable to scope
 */
static inline size_t vm_assembler_add_for_init(vm_assembler_t *code, VALUE var_name, uint8_t flags)
{
    /* Stack: collection on top, will be replaced by iterator state */
    /* No net stack change - collection consumed, iterator state pushed */
    size_t offset = vm_assembler_current_offset(code);
    uint16_t index = vm_assembler_write_ruby_constant(code, var_name);
    uint8_t *instructions = c_buffer_extend_for_write(&code->instructions, 4);
    instructions[0] = OP_FOR_INIT;
    instructions[1] = index >> 8;
    instructions[2] = (uint8_t)index;
    instructions[3] = flags;
    return offset;
}

/*
 * OP_FOR_NEXT: Get next item or jump if done
 * Operands: int16 done_offset (relative jump if iteration complete)
 * Stack: [iterator_state] -> [iterator_state] (unchanged)
 * - Increments iterator index
 * - Updates forloop drop properties
 * - Sets loop variable in scope
 * - Jumps to done_offset if no more items
 */
static inline size_t vm_assembler_add_for_next(vm_assembler_t *code)
{
    /* Stack unchanged */
    return vm_assembler_add_jump_placeholder(code, OP_FOR_NEXT);
}

/*
 * OP_FOR_CLEANUP: Cleanup after for loop
 * Operands: none
 * Stack: [iterator_state] -> []
 * - Pops iterator state from stack
 * - Removes forloop variable from scope
 * - Restores parent forloop if any
 */
static inline void vm_assembler_add_for_cleanup(vm_assembler_t *code)
{
    code->stack_size--; /* pops iterator state */
    vm_assembler_write_opcode(code, OP_FOR_CLEANUP);
}

/*
 * OP_DUP: Duplicate top of stack
 * Operands: none
 * Stack: [value] -> [value, value]
 */
static inline void vm_assembler_add_dup(vm_assembler_t *code)
{
    code->stack_size++; /* duplicates top value */
    vm_assembler_write_opcode(code, OP_DUP);
}

/*
 * OP_POP_DISCARD: Pop and discard top of stack
 * Operands: none
 * Stack: [value] -> []
 */
static inline void vm_assembler_add_pop_discard(vm_assembler_t *code)
{
    code->stack_size--; /* pops and discards top value */
    vm_assembler_write_opcode(code, OP_POP_DISCARD);
}

#endif
