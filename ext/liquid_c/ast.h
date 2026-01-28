#ifndef LIQUID_AST_H
#define LIQUID_AST_H

#include <ruby.h>
#include <stdbool.h>
#include "arena.h"
#include "vm_assembler.h"

/*
 * AST node structures for Liquid template parsing.
 * All nodes are allocated from an arena for efficient memory management.
 */

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
    AST_LIQUID_TAG,     /* {% liquid %} tag containing multiple statements */
} ast_node_type_t;

/* Forward declarations */
typedef struct ast_node ast_node_t;
typedef struct ast_node_list ast_node_list_t;
typedef struct ast_condition ast_condition_t;
typedef struct ast_branch ast_branch_t;
typedef struct ast_for_params ast_for_params_t;

/* List of AST nodes (dynamically growable) */
struct ast_node_list {
    ast_node_t **nodes;
    size_t count;
    size_t capacity;
};

/* Comparison operators */
typedef enum comparison_op {
    CMP_NONE = 0,
    CMP_EQ,             /* == */
    CMP_NE,             /* != or <> */
    CMP_LT,             /* < */
    CMP_GT,             /* > */
    CMP_LE,             /* <= */
    CMP_GE,             /* >= */
    CMP_CONTAINS,       /* contains */
} comparison_op_t;

/* Logical operators */
typedef enum logical_op {
    LOGIC_NONE = 0,
    LOGIC_AND,          /* and */
    LOGIC_OR,           /* or */
} logical_op_t;

/* Condition for if/unless/elsif */
struct ast_condition {
    vm_assembler_t left_expr;       /* Left expression bytecode */
    comparison_op_t comparison_op;  /* Comparison operator (CMP_NONE if just truthy check) */
    vm_assembler_t right_expr;      /* Right expression bytecode (if comparison) */
    logical_op_t logical_op;        /* LOGIC_NONE, LOGIC_AND, or LOGIC_OR */
    struct ast_condition *next;     /* Chained condition (for and/or) */
};

/* Branch for if/elsif/else or when/else */
struct ast_branch {
    ast_condition_t *condition;     /* NULL for else branch */
    ast_node_list_t body;           /* Branch body */
    struct ast_branch *next;        /* Next branch (elsif/when/else) */
};

/* For loop parameters */
struct ast_for_params {
    vm_assembler_t limit_expr;      /* limit: expression */
    vm_assembler_t offset_expr;     /* offset: expression */
    bool has_limit;
    bool has_offset;
    bool reversed;
};

/* Union of node-specific data */
typedef union ast_node_data {
    /* AST_TEMPLATE */
    struct {
        ast_node_list_t children;
    } template;

    /* AST_RAW */
    struct {
        const char *text;
        size_t length;
        bool lstrip;                /* Strip leading whitespace */
        bool rstrip;                /* Strip trailing whitespace */
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
        bool has_else;
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
        VALUE *param_names;         /* Array of parameter names */
        vm_assembler_t *param_exprs; /* Array of parameter expressions */
        size_t param_count;
    } include;

    /* AST_ECHO */
    struct {
        vm_assembler_t expr;
        unsigned int line_number;
    } echo;

    /* AST_COMMENT - no extra data needed */

    /* AST_BREAK, AST_CONTINUE - no extra data needed */

    /* AST_CUSTOM_TAG */
    struct {
        VALUE tag_name;             /* Tag name as Ruby symbol */
        VALUE markup;               /* Raw markup string */
        VALUE tag_obj;              /* Ruby tag object (after parse) */
    } custom_tag;

    /* AST_LIQUID_TAG */
    struct {
        ast_node_list_t statements; /* List of statements in liquid tag */
    } liquid_tag;
} ast_node_data_t;

/* Main AST node structure */
struct ast_node {
    ast_node_type_t type;
    ast_node_data_t data;
    unsigned int line_number;       /* Source line for error reporting */
};

/* Initialize a node list */
void ast_node_list_init(ast_node_list_t *list);

/* Append a node to a list (allocates from arena) */
void ast_node_list_append(ast_node_list_t *list, ast_node_t *node, arena_t *arena);

/* Allocate a new AST node from arena */
ast_node_t *ast_node_alloc(arena_t *arena, ast_node_type_t type, unsigned int line_number);

/* Allocate a new condition from arena */
ast_condition_t *ast_condition_alloc(arena_t *arena);

/* Allocate a new branch from arena */
ast_branch_t *ast_branch_alloc(arena_t *arena);

/* Mark Ruby VALUEs in AST for GC */
void ast_gc_mark(ast_node_t *node);

/* Mark condition for GC */
void ast_condition_gc_mark(ast_condition_t *condition);

/* Mark branch for GC */
void ast_branch_gc_mark(ast_branch_t *branch);

/* Mark node list for GC */
void ast_node_list_gc_mark(ast_node_list_t *list);

/* Get human-readable node type name */
const char *ast_node_type_name(ast_node_type_t type);

/* Initialize vm_assembler in AST nodes */
void ast_init_assembler(vm_assembler_t *assembler);

/* Free vm_assembler in AST nodes */
void ast_free_assembler(vm_assembler_t *assembler);

#endif /* LIQUID_AST_H */
