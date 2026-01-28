# Opcode Implementation Checklist

This document tracks existing opcodes vs. new opcodes needed for the template parser.

## Existing Opcodes (in vm_assembler.h)

These opcodes are already implemented and can be reused:

### Control Flow
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_LEAVE` | none | - | Exit VM execution |
| `OP_JUMP_FWD` | uint8 size | - | Jump forward (skip bytes), used for blank string removal |
| `OP_JUMP_FWD_W` | uint24 size | - | Wide forward jump |

### Stack Operations
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_PUSH_NIL` | none | +1 | Push nil |
| `OP_PUSH_TRUE` | none | +1 | Push true |
| `OP_PUSH_FALSE` | none | +1 | Push false |
| `OP_PUSH_INT8` | int8 | +1 | Push 8-bit integer |
| `OP_PUSH_INT16` | int16 (BE) | +1 | Push 16-bit integer |
| `OP_PUSH_CONST` | uint16 idx | +1 | Push constant from table |

### Variable Access
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_FIND_STATIC_VAR` | uint16 idx | +1 | Find variable by constant name |
| `OP_FIND_VAR` | none | 0 (pop 1, push 1) | Find variable by stack key |
| `OP_LOOKUP_CONST_KEY` | uint16 idx | 0 (pop 1, push 1) | Lookup by constant key |
| `OP_LOOKUP_KEY` | none | -1 (pop 2, push 1) | Lookup by stack key |
| `OP_LOOKUP_COMMAND` | uint16 idx | 0 (pop 1, push 1) | Lookup .size/.first/.last |

### Data Construction
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_NEW_INT_RANGE` | none | -1 (pop 2, push 1) | Create range from stack values |
| `OP_HASH_NEW` | uint8 size | -(size*2-1) | Create hash from stack pairs |

### Filters
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_FILTER` | uint16 idx | -n+1 | Apply filter (name+argc in constant) |
| `OP_BUILTIN_FILTER` | uint8 idx, uint8 argc | -n+1 | Apply builtin filter |

### Output
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_WRITE_RAW` | uint8 size, bytes | - | Write raw text (<=255 bytes) |
| `OP_WRITE_RAW_W` | uint24 size, bytes | - | Write raw text (wide) |
| `OP_WRITE_NODE` | uint16 idx | - | Render Ruby node object |
| `OP_POP_WRITE` | none | -1 | Pop and write to output |
| `OP_WRITE_RAW_SKIP` | ? | - | (appears unused) |

### Error Handling
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_RENDER_VARIABLE_RESCUE` | uint24 line | - | Setup rescue for variable render |

---

## New Opcodes Needed

These opcodes must be added for the template parser:

### Conditional Jumps (HIGH PRIORITY)
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_JUMP` | int16 offset | - | Unconditional relative jump |
| `OP_JUMP_W` | int24 offset | - | Wide unconditional jump |
| `OP_JUMP_IF_FALSE` | int16 offset | -1 | Jump if top is falsy (Liquid rules) |
| `OP_JUMP_IF_FALSE_W` | int24 offset | -1 | Wide conditional jump |
| `OP_JUMP_IF_TRUE` | int16 offset | -1 | Jump if top is truthy |
| `OP_JUMP_IF_TRUE_W` | int24 offset | -1 | Wide version |

**Note**: Existing `OP_JUMP_FWD`/`OP_JUMP_FWD_W` only jump forward by skipping bytes. New jump opcodes need signed offsets for backward jumps (loops) and conditional logic.

### Comparison Operators (HIGH PRIORITY)
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_CMP_EQ` | none | -1 (pop 2, push 1) | `==` comparison |
| `OP_CMP_NE` | none | -1 | `!=` comparison |
| `OP_CMP_LT` | none | -1 | `<` comparison |
| `OP_CMP_GT` | none | -1 | `>` comparison |
| `OP_CMP_LE` | none | -1 | `<=` comparison |
| `OP_CMP_GE` | none | -1 | `>=` comparison |
| `OP_CMP_CONTAINS` | none | -1 | `contains` check |

### Logical Operators
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_NOT` | none | 0 (pop 1, push 1) | Liquid logical not |
| `OP_TRUTHY` | none | 0 (pop 1, push 1) | Convert to Liquid boolean |

**Note**: `and`/`or` don't need opcodes - they use short-circuit evaluation with jumps.

### For Loop Support (HIGH PRIORITY)
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_FOR_INIT` | uint16 var_idx, flags | 0 | Initialize iterator + forloop object |
| `OP_FOR_NEXT` | int16 done_offset | +1 | Get next item or jump if done |
| `OP_FOR_CLEANUP` | none | - | Cleanup forloop, restore parent |

**Design consideration**: The forloop object needs to be accessible as a variable. Options:
1. Store in context's scopes (cleaner, matches Ruby)
2. Keep on VM stack (faster, but complex)

Recommend option 1 for compatibility.

### Variable Assignment (MEDIUM PRIORITY)
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_ASSIGN` | uint16 var_idx | -1 | Assign top of stack to variable |
| `OP_CAPTURE_START` | none | - | Start capturing output to buffer |
| `OP_CAPTURE_END` | uint16 var_idx | - | End capture, assign to variable |

### Counter Operations (MEDIUM PRIORITY)
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_INCREMENT` | uint16 var_idx | - | Increment counter, write value |
| `OP_DECREMENT` | uint16 var_idx | - | Decrement counter, write value |

### Cycle Support (MEDIUM PRIORITY)
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_CYCLE` | uint16 group_idx, uint8 count | -count+1 | Cycle through values |

### Loop Control (MEDIUM PRIORITY)
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_BREAK` | none | - | Break from innermost loop |
| `OP_CONTINUE` | none | - | Continue to next iteration |

**Design consideration**: These need to know the loop context. Options:
1. Emit as jumps during codegen (simpler, recommended)
2. Runtime loop stack lookup (more flexible)

Recommend option 1 - resolve break/continue to actual jump targets during code generation.

### Case Statement (MEDIUM PRIORITY)
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_CASE_CMP` | none | -1 (pop 2, push 1) | Compare case target with when value |

**Note**: Could potentially reuse `OP_CMP_EQ`, but Liquid's case uses `===` semantics in Ruby. Need to verify if `==` is sufficient or if we need Ruby's case equality.

### Tablerow Support (LOW PRIORITY)
| Opcode | Operands | Stack Effect | Description |
|--------|----------|--------------|-------------|
| `OP_TABLEROW_INIT` | uint16 var_idx, flags | 0 | Initialize tablerow iterator |
| `OP_TABLEROW_NEXT` | int16 done_offset | +1 | Get next item or jump |
| `OP_TABLEROW_COL_START` | none | - | Write `<td>` with class |
| `OP_TABLEROW_COL_END` | none | - | Write `</td>`, maybe `</tr><tr>` |
| `OP_TABLEROW_CLEANUP` | none | - | Write final `</tr>` if needed |

---

## Naming Considerations

### Consistency with existing names:
- Use `_W` suffix for wide (24-bit) variants (matches `OP_WRITE_RAW_W`, `OP_JUMP_FWD_W`)
- Use `OP_` prefix for all opcodes
- Use `CMP_` prefix for comparisons

### Potential conflicts:
- `OP_JUMP_FWD` exists but only skips forward. New `OP_JUMP` should support signed offsets for backward jumps
- Consider renaming existing forward jumps to `OP_SKIP`/`OP_SKIP_W` for clarity, but this risks breaking existing serialized bytecode

### Encoding recommendations:
- 16-bit offsets as signed int16 (range: -32768 to +32767)
- 24-bit offsets as signed int24 (range: -8388608 to +8388607)
- Big-endian encoding to match existing `OP_PUSH_INT16`

---

## Implementation Order

### Phase 1: Control Flow (enables if/unless)
1. `OP_CMP_*` (all 7 comparison operators)
2. `OP_JUMP_IF_FALSE` / `OP_JUMP_IF_FALSE_W`
3. `OP_JUMP` / `OP_JUMP_W`
4. `OP_TRUTHY`, `OP_NOT`

### Phase 2: Iteration (enables for)
5. `OP_FOR_INIT`, `OP_FOR_NEXT`, `OP_FOR_CLEANUP`
6. `OP_BREAK`, `OP_CONTINUE` (or resolve to jumps)

### Phase 3: Variables (enables assign/capture)
7. `OP_ASSIGN`
8. `OP_CAPTURE_START`, `OP_CAPTURE_END`
9. `OP_INCREMENT`, `OP_DECREMENT`

### Phase 4: Remaining
10. `OP_CYCLE`
11. `OP_CASE_CMP` (if needed beyond `OP_CMP_EQ`)
12. `OP_TABLEROW_*`

---

## VM Implementation Notes

### Liquid Truthiness
Only `nil` and `false` are falsy in Liquid:
```c
static inline bool liquid_is_truthy(VALUE obj) {
    return obj != Qnil && obj != Qfalse;
}
```

### Comparison Implementation
Liquid comparisons should match Ruby semantics:
```c
case OP_CMP_EQ: {
    VALUE b = vm_stack_pop(vm);
    VALUE a = vm_stack_pop(vm);
    vm_stack_push(vm, rb_equal(a, b) ? Qtrue : Qfalse);
    break;
}
```

### Jump Offset Encoding
```c
// Write signed 16-bit offset
static inline void vm_assembler_write_int16(vm_assembler_t *code, int16_t offset) {
    uint8_t *p = c_buffer_extend_for_write(&code->instructions, 2);
    p[0] = (offset >> 8) & 0xFF;
    p[1] = offset & 0xFF;
}

// Read signed 16-bit offset in VM
static inline int16_t read_int16(const uint8_t *ip) {
    return (int16_t)((ip[0] << 8) | ip[1]);
}
```
