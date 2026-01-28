#ifndef LIQUID_CODEGEN_H
#define LIQUID_CODEGEN_H

#include <ruby.h>
#include "ast.h"
#include "vm_assembler.h"

/*
 * Code generator for Liquid AST.
 * Compiles AST nodes to VM bytecode.
 */

/* Maximum number of break/continue statements per loop */
#define MAX_LOOP_BREAKS 64

/* Loop context for break/continue handling */
typedef struct loop_context {
    size_t continue_target;         /* Where continue jumps to (FOR_NEXT) */
    size_t break_jumps[MAX_LOOP_BREAKS];  /* Offsets of break jump instructions to patch */
    size_t break_jump_count;        /* Number of break jumps */
    struct loop_context *outer;     /* Enclosing loop context */
} loop_context_t;

/* Jump patch for forward references */
typedef struct jump_patch {
    size_t instruction_offset;      /* Offset of jump instruction */
    size_t target_label;            /* Label ID to jump to */
    struct jump_patch *next;
} jump_patch_t;

/* Code generator state */
typedef struct codegen {
    vm_assembler_t *code;
    VALUE code_obj;                 /* Ruby wrapper object for GC */

    /* Loop context for break/continue */
    loop_context_t *current_loop;

    /* Arena for temporary allocations */
    arena_t *arena;

    /* Statistics */
    unsigned int render_score;
    bool is_blank;
} codegen_t;

/* Initialize code generator */
void codegen_init(codegen_t *gen, vm_assembler_t *code, VALUE code_obj, arena_t *arena);

/* Generate code for an AST node */
void codegen_node(codegen_t *gen, ast_node_t *node);

/* Generate code for a node list */
void codegen_node_list(codegen_t *gen, ast_node_list_t *list);

/* Generate code for template root */
void codegen_template(codegen_t *gen, ast_node_t *root);

/* Get render score after code generation */
static inline unsigned int codegen_render_score(codegen_t *gen)
{
    return gen->render_score;
}

/* Check if generated code is blank (only whitespace) */
static inline bool codegen_is_blank(codegen_t *gen)
{
    return gen->is_blank;
}

/* Mark codegen for GC */
void codegen_gc_mark(codegen_t *gen);

/* Module initialization */
void liquid_define_codegen(void);

#endif /* LIQUID_CODEGEN_H */
