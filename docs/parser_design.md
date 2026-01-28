# Liquid Template Parser Design Document

## Overview

This document describes the architecture for a C-based parser for Liquid templates that handles control flow tags (if/unless/for/case/tablerow/etc.). The parser integrates with the existing liquid-c tokenizer and VM infrastructure.

## Current Architecture Analysis

### Existing Components

1. **Tokenizer** (`tokenizer.c`): Breaks templates into tokens:
   - `TOKEN_RAW` - Raw text between tags
   - `TOKEN_TAG` - `{% ... %}` constructs
   - `TOKEN_VARIABLE` - `{{ ... }}` constructs
   - `TOKEN_INVALID` - Malformed tokens

2. **Lexer** (`lexer.c`): Lexes expression content within tags:
   - Identifiers, numbers, strings
   - Operators: comparison, dots, pipes, etc.
   - Produces `lexer_token_t` with type and value pointers

3. **Parser** (`parser.c`): Parses expressions only:
   - Variable lookups, filters, ranges
   - Compiles directly to VM bytecode
   - No AST - direct code generation

4. **VM Assembler** (`vm_assembler.c`): Bytecode generation:
   - Stack-based operations
   - Constants table with deduplication
   - Instructions stored in `c_buffer_t`

5. **VM** (`liquid_vm.c`): Stack-based bytecode interpreter:
   - Renders to output buffer
   - Evaluates expressions
   - Handles error recovery

6. **Block Body** (`block.c`): Current template parsing:
   - Parses raw text, variables, and tags
   - Delegates tag parsing to Ruby via `rb_funcall`
   - Control flow tags handled entirely by Ruby

### Current Limitations

- Control flow tags (if/for/case) delegate to Ruby for parsing and execution
- Each nested block requires Ruby method calls
- No optimization across control flow boundaries
- Tag body execution goes through `OP_WRITE_NODE` which calls Ruby

## Proposed Parser Architecture

### Design Goals

1. Parse all control flow tags in C
2. Generate optimized bytecode for entire templates
3. Minimize Ruby calls during rendering
4. Maintain compatibility with existing VM infrastructure
5. Support custom tags via Ruby fallback

### Grammar Definition

```ebnf
template      = { raw_text | output | tag } ;
raw_text      = (* any text outside tags *) ;
output        = "{{" expression "}}" ;
tag           = "{%" tag_content "%}" ;

tag_content   = if_tag | unless_tag | case_tag | for_tag | tablerow_tag
              | assign_tag | capture_tag | increment_tag | decrement_tag
              | cycle_tag | include_tag | render_tag | echo_tag
              | liquid_tag | comment_tag | raw_tag | unknown_tag ;

(* Control Flow *)
if_tag        = "if" condition block { elsif_block } [ else_block ] "endif" ;
elsif_block   = "elsif" condition block ;
else_block    = "else" block ;
unless_tag    = "unless" condition block [ else_block ] "endunless" ;

case_tag      = "case" expression { when_block } [ else_block ] "endcase" ;
when_block    = "when" expression { "," expression } block ;

for_tag       = "for" identifier "in" expression [ for_params ] block
                [ else_block ] "endfor" ;
for_params    = { "limit:" expression | "offset:" expression | "reversed" } ;

tablerow_tag  = "tablerow" identifier "in" expression [ tablerow_params ] block
                "endtablerow" ;
tablerow_params = { "cols:" expression | "limit:" expression | "offset:" expression } ;

(* Variables *)
assign_tag    = "assign" identifier "=" expression ;
capture_tag   = "capture" identifier block "endcapture" ;
increment_tag = "increment" identifier ;
decrement_tag = "decrement" identifier ;

(* Iteration *)
cycle_tag     = "cycle" [ cycle_group ":" ] expression { "," expression } ;
cycle_group   = string | identifier ;

(* Template Inclusion *)
include_tag   = "include" expression [ include_params ] ;
render_tag    = "render" expression [ render_params ] ;
include_params = { "with" expression [ "as" identifier ]
                 | "for" expression [ "as" identifier ]
                 | identifier ":" expression } ;
render_params = include_params ;

(* Other *)
echo_tag      = "echo" expression ;
liquid_tag    = "liquid" { newline tag_line } ;
tag_line      = tag_name markup newline ;
comment_tag   = "comment" (* anything *) "endcomment" ;
raw_tag       = "raw" (* literal text *) "endraw" ;

(* Expressions - already implemented in parser.c *)
condition     = expression [ comparison expression ]
              | condition ("and" | "or") condition ;
comparison    = "==" | "!=" | "<" | ">" | "<=" | ">=" | "contains" ;
expression    = (* see existing parser.c implementation *) ;
```

### AST Node Structures

```c
/* Node types enumeration */
typedef enum ast_node_type {
    AST_TEMPLATE,       /* Root node containing list of children */
    AST_RAW,            /* Raw text output */
    AST_VARIABLE,       /* {{ expression }} */
    AST_IF,             /* if/elsif/else/endif */
    AST_UNLESS,         /* unless/else/endunless */
    AST_CASE,           /* case/when/else/endcase */
    AST_FOR,            /* for/else/endfor */
    AST_TABLEROW,       /* tablerow/endtablerow */
    AST_ASSIGN,         /* assign var = expr */
    AST_CAPTURE,        /* capture/endcapture */
    AST_INCREMENT,      /* increment var */
    AST_DECREMENT,      /* decrement var */
    AST_CYCLE,          /* cycle values */
    AST_INCLUDE,        /* include template */
    AST_RENDER,         /* render template */
    AST_ECHO,           /* echo expression */
    AST_COMMENT,        /* comment block (no output) */
    AST_BREAK,          /* break from for loop */
    AST_CONTINUE,       /* continue to next iteration */
    AST_CUSTOM_TAG,     /* Custom tag - delegate to Ruby */
} ast_node_type_t;

/* Forward declarations */
typedef struct ast_node ast_node_t;
typedef struct ast_node_list ast_node_list_t;

/* List of AST nodes */
struct ast_node_list {
    ast_node_t **nodes;
    size_t count;
    size_t capacity;
};

/* Condition for if/unless/elsif */
typedef struct ast_condition {
    vm_assembler_t left_expr;       /* Left expression bytecode */
    uint8_t comparison_op;          /* 0 if no comparison, else TOKEN_COMPARISON type */
    vm_assembler_t right_expr;      /* Right expression bytecode (if comparison) */
    uint8_t logical_op;             /* 0, 'and', or 'or' */
    struct ast_condition *next;     /* Chained condition */
} ast_condition_t;

/* Branch for if/elsif/else or when/else */
typedef struct ast_branch {
    ast_condition_t *condition;     /* NULL for else branch */
    ast_node_list_t body;           /* Branch body */
    struct ast_branch *next;        /* Next branch (elsif/when/else) */
} ast_branch_t;

/* For loop parameters */
typedef struct ast_for_params {
    vm_assembler_t limit_expr;      /* limit: expression */
    vm_assembler_t offset_expr;     /* offset: expression */
    bool has_limit;
    bool has_offset;
    bool reversed;
} ast_for_params_t;

/* Union of node-specific data */
typedef union ast_node_data {
    /* AST_RAW */
    struct {
        const char *text;
        size_t length;
    } raw;

    /* AST_VARIABLE */
    struct {
        vm_assembler_t expr;        /* Compiled expression with filters */
        unsigned int line_number;
    } variable;

    /* AST_IF, AST_UNLESS */
    struct {
        ast_branch_t *branches;     /* Linked list of branches */
    } conditional;

    /* AST_CASE */
    struct {
        vm_assembler_t target_expr; /* case <target> */
        ast_branch_t *branches;     /* when/else branches */
    } case_stmt;

    /* AST_FOR */
    struct {
        VALUE var_name;             /* Loop variable name (symbol) */
        vm_assembler_t collection;  /* Collection expression */
        ast_for_params_t params;
        ast_node_list_t body;
        ast_node_list_t else_body;  /* For empty collection */
    } for_loop;

    /* AST_TABLEROW */
    struct {
        VALUE var_name;
        vm_assembler_t collection;
        ast_for_params_t params;
        vm_assembler_t cols_expr;   /* cols: expression */
        bool has_cols;
        ast_node_list_t body;
    } tablerow;

    /* AST_ASSIGN */
    struct {
        VALUE var_name;             /* Variable name (symbol) */
        vm_assembler_t expr;
    } assign;

    /* AST_CAPTURE */
    struct {
        VALUE var_name;
        ast_node_list_t body;
    } capture;

    /* AST_INCREMENT, AST_DECREMENT */
    struct {
        VALUE var_name;
    } counter;

    /* AST_CYCLE */
    struct {
        VALUE group_name;           /* Optional group (Qnil if none) */
        vm_assembler_t *values;     /* Array of value expressions */
        size_t value_count;
    } cycle;

    /* AST_INCLUDE, AST_RENDER */
    struct {
        vm_assembler_t template_expr;
        VALUE variable_name;        /* "with" variable name (Qnil if none) */
        vm_assembler_t variable_expr;
        bool is_for_loop;           /* "for" instead of "with" */
        /* Named parameters stored in hash */
        vm_assembler_t params;      /* Hash of named params */
        size_t param_count;
    } include;

    /* AST_ECHO */
    struct {
        vm_assembler_t expr;
        unsigned int line_number;
    } echo;

    /* AST_CUSTOM_TAG */
    struct {
        VALUE tag_name;             /* Tag name as Ruby symbol */
        VALUE markup;               /* Raw markup string */
        VALUE tag_obj;              /* Ruby tag object (after parse) */
    } custom_tag;
} ast_node_data_t;

/* Main AST node structure */
struct ast_node {
    ast_node_type_t type;
    ast_node_data_t data;
    unsigned int line_number;       /* Source line for error reporting */
};
```

### Memory Management: Arena Allocator

To minimize allocation overhead and simplify cleanup, use arena allocation:

```c
/* Arena block for memory allocation */
typedef struct arena_block {
    struct arena_block *next;
    size_t size;
    size_t used;
    uint8_t data[];                 /* Flexible array member */
} arena_block_t;

/* Arena allocator */
typedef struct arena {
    arena_block_t *current;
    arena_block_t *first;
    size_t default_block_size;
} arena_t;

#define ARENA_DEFAULT_BLOCK_SIZE (64 * 1024)  /* 64KB blocks */

/* Initialize arena */
static inline void arena_init(arena_t *arena) {
    arena->current = NULL;
    arena->first = NULL;
    arena->default_block_size = ARENA_DEFAULT_BLOCK_SIZE;
}

/* Allocate from arena (8-byte aligned) */
void *arena_alloc(arena_t *arena, size_t size);

/* Allocate zeroed memory */
void *arena_calloc(arena_t *arena, size_t count, size_t size);

/* Duplicate string into arena */
const char *arena_strdup(arena_t *arena, const char *str, size_t len);

/* Free entire arena */
void arena_free(arena_t *arena);

/* Mark arena for GC (mark all Ruby VALUEs) */
void arena_gc_mark(arena_t *arena);
```

**Benefits of Arena Allocation:**
- Fast allocation (bump pointer)
- No individual frees needed
- Cache-friendly memory layout
- Simple cleanup (free entire arena)
- Reduced fragmentation

### New VM Opcodes for Control Flow

```c
enum opcode {
    /* Existing opcodes... */

    /* New control flow opcodes */
    OP_JUMP,              /* Unconditional jump: JUMP offset_16 */
    OP_JUMP_W,            /* Wide jump: JUMP_W offset_24 */
    OP_JUMP_IF_FALSE,     /* Conditional: JUMP_IF_FALSE offset_16 */
    OP_JUMP_IF_FALSE_W,   /* Wide conditional jump */
    OP_JUMP_IF_TRUE,      /* JUMP_IF_TRUE offset_16 */
    OP_JUMP_IF_TRUE_W,    /* Wide version */

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
    OP_TRUTHY,            /* Convert to boolean (Liquid truthiness) */

    /* For loop support */
    OP_FOR_INIT,          /* Initialize forloop object */
    OP_FOR_NEXT,          /* Advance iterator, push item or jump if done */
    OP_FOR_CLEANUP,       /* Cleanup forloop object */

    /* Variable operations */
    OP_ASSIGN,            /* Assign to variable: ASSIGN const_idx */
    OP_CAPTURE_START,     /* Start capture to buffer */
    OP_CAPTURE_END,       /* End capture, assign to variable */

    /* Counters */
    OP_INCREMENT,         /* Increment counter */
    OP_DECREMENT,         /* Decrement counter */

    /* Cycle */
    OP_CYCLE,             /* Cycle through values */

    /* Loop control */
    OP_BREAK,             /* Break from loop */
    OP_CONTINUE,          /* Continue to next iteration */

    /* Case support */
    OP_CASE_EQ,           /* Compare case target with when value */

    /* Tablerow support */
    OP_TABLEROW_INIT,
    OP_TABLEROW_NEXT,
    OP_TABLEROW_COL_START,
    OP_TABLEROW_COL_END,
    OP_TABLEROW_CLEANUP,
};
```

### Parser Structure

```c
/* Template parser state */
typedef struct template_parser {
    /* Input */
    tokenizer_t *tokenizer;
    VALUE tokenizer_obj;            /* Ruby tokenizer wrapper (for GC) */
    VALUE parse_context;            /* Ruby parse context */

    /* Arena for AST allocation */
    arena_t arena;

    /* Current parsing state */
    token_t current_token;
    parser_t expr_parser;           /* Reused for expression parsing */

    /* Error handling */
    jmp_buf error_jmp;
    VALUE error_exception;

    /* Output */
    ast_node_t *root;

    /* Statistics */
    unsigned int node_count;
    unsigned int max_depth;
} template_parser_t;

/* Initialize parser */
void template_parser_init(template_parser_t *parser,
                          VALUE tokenizer_obj,
                          VALUE parse_context);

/* Parse template, returns root AST node */
ast_node_t *template_parser_parse(template_parser_t *parser);

/* Free parser resources */
void template_parser_free(template_parser_t *parser);
```

### Code Generation

The code generator traverses the AST and emits bytecode:

```c
/* Code generator state */
typedef struct codegen {
    vm_assembler_t *code;
    VALUE code_obj;                 /* Ruby wrapper for GC */

    /* Loop context for break/continue */
    struct loop_context {
        size_t break_target;        /* Offset to patch */
        size_t continue_target;     /* Offset to patch */
        struct loop_context *outer;
    } *current_loop;

    /* Pending jump targets to patch */
    struct jump_patch {
        size_t instruction_offset;
        size_t target_offset;
        struct jump_patch *next;
    } *patches;
} codegen_t;

/* Generate code for AST */
void codegen_template(codegen_t *gen, ast_node_t *node);

/* Generate code for specific node types */
static void codegen_raw(codegen_t *gen, ast_node_t *node);
static void codegen_variable(codegen_t *gen, ast_node_t *node);
static void codegen_if(codegen_t *gen, ast_node_t *node);
static void codegen_for(codegen_t *gen, ast_node_t *node);
static void codegen_case(codegen_t *gen, ast_node_t *node);
/* ... etc ... */
```

### Integration with Existing Block Body

The new parser integrates with the existing `block_body_t` structure:

```c
/* Modified block.c to use new parser */
static tag_markup_t internal_block_body_parse(block_body_t *body,
                                               parse_context_t *parse_context)
{
    template_parser_t parser;
    template_parser_init(&parser, parse_context->tokenizer_obj,
                         parse_context->ruby_obj);

    /* Parse to AST */
    ast_node_t *ast = template_parser_parse(&parser);

    /* Generate bytecode */
    codegen_t gen;
    codegen_init(&gen, body->as.intermediate.code, body->obj);
    codegen_template(&gen, ast);

    /* Cleanup */
    template_parser_free(&parser);

    return (tag_markup_t){ Qnil, Qnil };
}
```

### Liquid Truthiness Implementation

Liquid has specific truthiness rules (only `nil` and `false` are falsy):

```c
/* Check Liquid truthiness */
static inline bool liquid_is_truthy(VALUE obj) {
    return obj != Qnil && obj != Qfalse;
}

/* VM implementation of OP_TRUTHY */
case OP_TRUTHY: {
    VALUE obj = vm_stack_pop(vm);
    vm_stack_push(vm, liquid_is_truthy(obj) ? Qtrue : Qfalse);
    break;
}
```

### For Loop Implementation

For loops require special handling for the `forloop` object:

```c
/* For loop context (pushed to context stack) */
typedef struct forloop {
    long length;
    long index;         /* 0-based */
    long index1;        /* 1-based */
    long rindex;        /* Reverse index */
    long rindex1;       /* Reverse index 1-based */
    bool first;
    bool last;
    VALUE parent;       /* Outer forloop or nil */
} forloop_t;

/* OP_FOR_INIT implementation */
case OP_FOR_INIT: {
    /* Stack: [collection] -> [iterator, forloop_obj] */
    VALUE collection = vm_stack_pop(vm);
    VALUE array = rb_funcall(collection, rb_intern("to_a"), 0);

    /* Apply limit/offset/reversed (from following bytes) */
    long offset = bytes_to_int16(ip); ip += 2;
    long limit = bytes_to_int16(ip); ip += 2;
    bool reversed = *ip++;

    /* ... apply transformations ... */

    forloop_t *forloop = create_forloop(vm, RARRAY_LEN(array));
    vm_stack_push(vm, (VALUE)array);
    vm_stack_push(vm, (VALUE)forloop);
    break;
}
```

### Error Handling

Parser errors use longjmp for clean unwinding:

```c
__attribute__((noreturn))
static void parser_error(template_parser_t *parser, const char *format, ...) {
    va_list args;
    va_start(args, format);

    char message[256];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    parser->error_exception = rb_exc_new_str(cLiquidSyntaxError,
        rb_sprintf("Liquid syntax error (line %u): %s",
                   parser->tokenizer->line_number, message));

    longjmp(parser->error_jmp, 1);
}

/* Parse with error handling */
ast_node_t *template_parser_parse(template_parser_t *parser) {
    if (setjmp(parser->error_jmp)) {
        /* Error occurred - cleanup and raise */
        template_parser_free(parser);
        rb_exc_raise(parser->error_exception);
    }

    return parse_template(parser);
}
```

### Custom Tag Fallback

Unknown tags fall back to Ruby:

```c
static ast_node_t *parse_unknown_tag(template_parser_t *parser,
                                      const char *name, size_t name_len,
                                      const char *markup, size_t markup_len) {
    VALUE tag_name = rb_enc_str_new(name, name_len, utf8_encoding);
    VALUE tag_class = rb_funcall(tag_registry, intern_square_brackets, 1, tag_name);

    if (tag_class == Qnil) {
        /* Unknown tag - return to caller for handling */
        return NULL;
    }

    VALUE markup_str = rb_enc_str_new(markup, markup_len, utf8_encoding);
    VALUE tag_obj = rb_funcall(tag_class, intern_parse, 4,
        tag_name, markup_str, parser->tokenizer_obj, parser->parse_context);

    ast_node_t *node = arena_alloc(&parser->arena, sizeof(ast_node_t));
    node->type = AST_CUSTOM_TAG;
    node->data.custom_tag.tag_name = tag_name;
    node->data.custom_tag.markup = markup_str;
    node->data.custom_tag.tag_obj = tag_obj;
    node->line_number = parser->tokenizer->line_number;

    return node;
}
```

## File Structure

New/modified files:

```
ext/liquid_c/
  template_parser.h     # Parser declarations
  template_parser.c     # Parser implementation
  ast.h                 # AST node structures
  ast.c                 # AST utilities
  arena.h               # Arena allocator declarations
  arena.c               # Arena allocator implementation
  codegen.h             # Code generator declarations
  codegen.c             # Code generator implementation
  vm_assembler.h        # Add new opcodes (modified)
  vm_assembler.c        # Implement new opcode helpers (modified)
  liquid_vm.c           # Implement new opcodes (modified)
  block.c               # Integrate new parser (modified)
```

## Implementation Phases

### Phase 1: Infrastructure
1. Implement arena allocator
2. Define AST structures
3. Add new VM opcodes (stubs)
4. Basic parser framework with error handling

### Phase 2: Expression Enhancements
1. Condition parsing (and/or/comparisons)
2. Condition code generation
3. Jump opcodes implementation

### Phase 3: Control Flow Tags
1. if/elsif/else/endif
2. unless/else/endunless
3. case/when/else/endcase

### Phase 4: Iteration Tags
1. for/else/endfor with forloop object
2. break/continue
3. tablerow/endtablerow
4. cycle

### Phase 5: Variable Tags
1. assign
2. capture/endcapture
3. increment/decrement

### Phase 6: Template Tags
1. include (basic)
2. render (basic)
3. Parameter passing

### Phase 7: Optimization & Polish
1. Jump optimization (remove unnecessary jumps)
2. Constant folding for conditions
3. Dead code elimination
4. Performance benchmarking
5. Memory usage optimization

## Performance Considerations

1. **Avoid Ruby calls during rendering**: All control flow in C
2. **Efficient jump encoding**: Use 16-bit offsets, widen to 24-bit only when needed
3. **Forloop object pooling**: Reuse forloop objects
4. **String interning**: Reuse variable name symbols
5. **Branch prediction hints**: Mark common paths
6. **Inline caching**: Cache method lookups for drops

## Testing Strategy

1. **Unit tests**: Each parser function, each opcode
2. **Integration tests**: Full template parsing and rendering
3. **Compatibility tests**: Compare output with Ruby implementation
4. **Fuzz testing**: Random templates for crash detection
5. **Performance tests**: Benchmark against Ruby implementation
6. **Memory tests**: Valgrind/ASAN for leak detection

## Backwards Compatibility

1. Custom tags continue to work via Ruby fallback
2. Error messages match existing format
3. Line numbers preserved for debugging
4. Profiler integration maintained
5. `nodelist` method returns compatible structure
