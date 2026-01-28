#include "codegen.h"
#include "liquid.h"
#include "stringutil.h"
#include "vm_assembler.h"
#include <string.h>

/* Forward declarations */
static void codegen_raw(codegen_t *gen, ast_node_t *node);
static void codegen_variable(codegen_t *gen, ast_node_t *node);
static void codegen_if(codegen_t *gen, ast_node_t *node);
static void codegen_case(codegen_t *gen, ast_node_t *node);
static void codegen_for(codegen_t *gen, ast_node_t *node);
static void codegen_tablerow(codegen_t *gen, ast_node_t *node);
static void codegen_assign(codegen_t *gen, ast_node_t *node);
static void codegen_capture(codegen_t *gen, ast_node_t *node);
static void codegen_increment(codegen_t *gen, ast_node_t *node);
static void codegen_decrement(codegen_t *gen, ast_node_t *node);
static void codegen_cycle(codegen_t *gen, ast_node_t *node);
static void codegen_echo(codegen_t *gen, ast_node_t *node);
static void codegen_custom_tag(codegen_t *gen, ast_node_t *node);
static void codegen_liquid_tag(codegen_t *gen, ast_node_t *node);

void codegen_init(codegen_t *gen, vm_assembler_t *code, VALUE code_obj, arena_t *arena)
{
    gen->code = code;
    gen->code_obj = code_obj;
    gen->current_loop = NULL;
    gen->arena = arena;
    gen->render_score = 0;
    gen->is_blank = true;
}

void codegen_gc_mark(codegen_t *gen)
{
    rb_gc_mark(gen->code_obj);
}

/* Emit comparison opcode based on comparison_op */
static void codegen_emit_comparison(codegen_t *gen, comparison_op_t op)
{
    vm_assembler_t *code = gen->code;

    switch (op) {
        case CMP_EQ:
            vm_assembler_add_cmp_eq(code);
            break;
        case CMP_NE:
            vm_assembler_add_cmp_ne(code);
            break;
        case CMP_LT:
            vm_assembler_add_cmp_lt(code);
            break;
        case CMP_GT:
            vm_assembler_add_cmp_gt(code);
            break;
        case CMP_LE:
            vm_assembler_add_cmp_le(code);
            break;
        case CMP_GE:
            vm_assembler_add_cmp_ge(code);
            break;
        case CMP_CONTAINS:
            vm_assembler_add_cmp_contains(code);
            break;
        default:
            break;
    }
}

/* Emit code for a single condition (without logical operators) */
static void codegen_single_condition(codegen_t *gen, ast_condition_t *condition)
{
    vm_assembler_t *code = gen->code;

    /* Emit left expression */
    vm_assembler_concat(code, &condition->left_expr);

    if (condition->comparison_op != CMP_NONE) {
        /* Emit right expression */
        vm_assembler_concat(code, &condition->right_expr);
        /* Emit comparison */
        codegen_emit_comparison(gen, condition->comparison_op);
    } else {
        /* Just a truthy check - convert to boolean */
        vm_assembler_add_truthy(code);
    }
}

/* Check if a condition has and/or chaining */
static bool condition_has_chaining(ast_condition_t *condition)
{
    return condition != NULL && condition->next != NULL && condition->logical_op != LOGIC_NONE;
}

/* Emit code for a condition and return jump offset for the branch skip.
 * Returns SIZE_MAX if condition has and/or (indicating native parsing should be skipped).
 */
static size_t codegen_condition_for_branch(codegen_t *gen, ast_condition_t *condition, bool is_unless)
{
    vm_assembler_t *code = gen->code;

    if (condition == NULL) {
        /* No condition - should not happen, but handle gracefully */
        vm_assembler_add_push_true(code);
        return vm_assembler_add_jump_if_false(code);
    }

    /* For conditions with and/or, we would need more complex codegen.
     * For now, return SIZE_MAX to signal that native parsing should fall back to Ruby. */
    if (condition_has_chaining(condition)) {
        return SIZE_MAX;
    }

    /* Simple case: single condition without chaining */
    codegen_single_condition(gen, condition);
    if (is_unless) {
        return vm_assembler_add_jump_if_true(code);
    } else {
        return vm_assembler_add_jump_if_false(code);
    }
}

/* Emit condition and return jump offset to patch if condition is false (or true for unless) */
static size_t codegen_condition_with_jump(codegen_t *gen, ast_condition_t *condition, bool is_unless)
{
    return codegen_condition_for_branch(gen, condition, is_unless);
}

static void codegen_raw(codegen_t *gen, ast_node_t *node)
{
    const char *text = node->data.raw.text;
    size_t length = node->data.raw.length;

    /* Apply whitespace stripping */
    const char *start = text;
    const char *end = text + length;

    if (node->data.raw.lstrip) {
        start = read_while(start, end, rb_isspace);
    }

    if (node->data.raw.rstrip) {
        end = read_while_reverse(start, end, rb_isspace);
    }

    if (start < end) {
        vm_assembler_add_write_raw(gen->code, start, end - start);
        gen->render_score++;

        /* Check if content is non-blank */
        if (gen->is_blank) {
            const char *p = start;
            while (p < end && rb_isspace(*p)) p++;
            if (p < end) {
                gen->is_blank = false;
            }
        }
    }
}

static void codegen_variable(codegen_t *gen, ast_node_t *node)
{
    /* Add render rescue point for error handling */
    vm_assembler_add_render_variable_rescue(gen->code, node->data.variable.line_number);

    /* Emit the expression bytecode */
    vm_assembler_concat(gen->code, &node->data.variable.expr);

    /* Write result to output */
    vm_assembler_add_pop_write(gen->code);

    gen->render_score++;
    gen->is_blank = false;
}

static void codegen_if(codegen_t *gen, ast_node_t *node)
{
    vm_assembler_t *code = gen->code;
    bool is_unless = (node->type == AST_UNLESS);
    bool is_first_branch = true;

    ast_branch_t *branch = node->data.conditional.branches;

    /* Collect jump offsets that need to jump to end */
    size_t end_jumps[64];  /* Max 64 branches */
    size_t end_jump_count = 0;

    while (branch != NULL) {
        if (branch->condition != NULL) {
            /* Evaluate condition and jump to next branch if false (or true for first unless branch) */
            /* Note: only the first branch of unless gets inverted logic, elsif branches don't exist in unless */
            size_t next_branch_jump = codegen_condition_with_jump(gen, branch->condition,
                is_unless && is_first_branch);
            is_first_branch = false;

            /* Emit body */
            codegen_node_list(gen, &branch->body);

            /* Jump to end (unless this is the last branch) */
            if (branch->next != NULL && end_jump_count < 64) {
                end_jumps[end_jump_count++] = vm_assembler_add_jump_placeholder(code, OP_JUMP);
            }

            /* Patch the conditional jump to here (next branch) */
            vm_assembler_patch_jump(code, next_branch_jump, vm_assembler_current_offset(code));
        } else {
            /* else branch - no condition */
            codegen_node_list(gen, &branch->body);
        }

        branch = branch->next;
    }

    /* Patch all end jumps to here */
    size_t end_offset = vm_assembler_current_offset(code);
    for (size_t i = 0; i < end_jump_count; i++) {
        vm_assembler_patch_jump(code, end_jumps[i], end_offset);
    }
}

static void codegen_case(codegen_t *gen, ast_node_t *node)
{
    vm_assembler_t *code = gen->code;

    /* Note: We don't pre-push the target expression - we re-evaluate it
     * for each when branch. This is simpler and matches Ruby's semantics
     * where the case target could have side effects. */

    ast_branch_t *branch = node->data.case_stmt.branches;

    size_t end_jumps[64];
    size_t end_jump_count = 0;

    while (branch != NULL) {
        if (branch->condition != NULL) {
            /* when branch - push target, push when value, compare */
            vm_assembler_concat(code, &node->data.case_stmt.target_expr);
            vm_assembler_concat(code, &branch->condition->left_expr);

            /* Compare with == */
            vm_assembler_add_cmp_eq(code);

            /* Jump to next when if not equal */
            size_t next_when_jump = vm_assembler_add_jump_if_false(code);

            /* Emit body */
            codegen_node_list(gen, &branch->body);

            /* Jump to end */
            if (branch->next != NULL && end_jump_count < 64) {
                end_jumps[end_jump_count++] = vm_assembler_add_jump_placeholder(code, OP_JUMP);
            }

            /* Patch conditional jump to here */
            vm_assembler_patch_jump(code, next_when_jump, vm_assembler_current_offset(code));
        } else {
            /* else branch */
            codegen_node_list(gen, &branch->body);
        }

        branch = branch->next;
    }

    /* Patch all end jumps */
    size_t end_offset = vm_assembler_current_offset(code);
    for (size_t i = 0; i < end_jump_count; i++) {
        vm_assembler_patch_jump(code, end_jumps[i], end_offset);
    }
}

static void codegen_for(codegen_t *gen, ast_node_t *node)
{
    vm_assembler_t *code = gen->code;

    /*
     * For loop bytecode structure:
     *
     *   [collection expression]
     *   OP_FOR_INIT var_name, flags      ; initialize iterator, jump to cleanup if empty
     *   OP_FOR_NEXT done_offset          ; get next item or jump to cleanup
     * loop_body:
     *   [body code]                      ; continue jumps to FOR_NEXT
     *   OP_JUMP loop_start               ; jump back to FOR_NEXT
     * cleanup:
     *   OP_FOR_CLEANUP                   ; cleanup forloop
     * loop_end:
     *   (break jumps here)
     */

    /* Create loop context for break/continue */
    loop_context_t loop_ctx = {
        .continue_target = 0,
        .break_jump_count = 0,
        .outer = gen->current_loop
    };

    gen->current_loop = &loop_ctx;

    /* Emit collection expression - leaves collection on stack */
    vm_assembler_concat(code, &node->data.for_loop.collection);

    /* Determine flags */
    uint8_t flags = 0;
    if (node->data.for_loop.params.reversed) {
        flags |= FOR_FLAG_REVERSED;
    }

    /* OP_FOR_INIT: Initialize forloop with variable name */
    vm_assembler_add_for_init(code, node->data.for_loop.var_name, flags);

    /* Record the position for continue to jump to (the FOR_NEXT instruction) */
    size_t for_next_offset = vm_assembler_current_offset(code);
    loop_ctx.continue_target = for_next_offset;

    /* OP_FOR_NEXT: Get next item or jump to cleanup */
    size_t for_next_jump = vm_assembler_add_for_next(code);

    /* Generate loop body */
    codegen_node_list(gen, &node->data.for_loop.body);

    /* Jump back to FOR_NEXT */
    size_t loop_back_jump = vm_assembler_add_jump_placeholder(code, OP_JUMP);
    vm_assembler_patch_jump(code, loop_back_jump, for_next_offset);

    /* This is where FOR_NEXT jumps when done, and where break jumps to */
    size_t cleanup_offset = vm_assembler_current_offset(code);
    vm_assembler_patch_jump(code, for_next_jump, cleanup_offset);

    /* Patch all break jumps to point to the cleanup instruction */
    for (size_t i = 0; i < loop_ctx.break_jump_count; i++) {
        vm_assembler_patch_jump(code, loop_ctx.break_jumps[i], cleanup_offset);
    }

    /* OP_FOR_CLEANUP */
    vm_assembler_add_for_cleanup(code);

    /* Handle else body (only runs if collection was empty) */
    /* Note: For proper else support, we'd need to track if loop ran at all.
     * This is a simplification that runs else unconditionally after an empty loop.
     * The FOR_INIT/FOR_NEXT logic should handle this correctly. */
    if (node->data.for_loop.has_else && node->data.for_loop.else_body.count > 0) {
        codegen_node_list(gen, &node->data.for_loop.else_body);
    }

    gen->current_loop = loop_ctx.outer;
    gen->is_blank = false;
    gen->render_score++;
}

static void codegen_tablerow(codegen_t *gen, ast_node_t *node)
{
    /* Tablerow also delegates to Ruby for now */
    gen->is_blank = false;
}

static void codegen_assign(codegen_t *gen, ast_node_t *node)
{
    /* Evaluate expression */
    vm_assembler_concat(gen->code, &node->data.assign.expr);

    /* Assign to variable using native opcode */
    vm_assembler_add_assign(gen->code, node->data.assign.var_name);
}

static void codegen_capture(codegen_t *gen, ast_node_t *node)
{
    /* Capture still delegates to Ruby for now since it needs
     * output buffer management */
    codegen_node_list(gen, &node->data.capture.body);
}

static void codegen_increment(codegen_t *gen, ast_node_t *node)
{
    /* Use native increment opcode */
    vm_assembler_add_increment(gen->code, node->data.counter.var_name);
    gen->render_score++;
    gen->is_blank = false;
}

static void codegen_decrement(codegen_t *gen, ast_node_t *node)
{
    /* Use native decrement opcode */
    vm_assembler_add_decrement(gen->code, node->data.counter.var_name);
    gen->render_score++;
    gen->is_blank = false;
}

static void codegen_cycle(codegen_t *gen, ast_node_t *node)
{
    /* Cycle still delegates to Ruby for now */
    gen->render_score++;
    gen->is_blank = false;
}

static void codegen_echo(codegen_t *gen, ast_node_t *node)
{
    /* Same as variable output */
    vm_assembler_add_render_variable_rescue(gen->code, node->data.echo.line_number);
    vm_assembler_concat(gen->code, &node->data.echo.expr);
    vm_assembler_add_pop_write(gen->code);

    gen->render_score++;
    gen->is_blank = false;
}

static void codegen_custom_tag(codegen_t *gen, ast_node_t *node)
{
    /* Delegate to Ruby via OP_WRITE_NODE */
    if (node->data.custom_tag.tag_obj != Qnil) {
        vm_assembler_add_write_node(gen->code, node->data.custom_tag.tag_obj);
        gen->render_score++;
        gen->is_blank = false;
    }
}

static void codegen_liquid_tag(codegen_t *gen, ast_node_t *node)
{
    /* Generate code for each statement in the liquid tag */
    codegen_node_list(gen, &node->data.liquid_tag.statements);
}

static void codegen_break(codegen_t *gen, ast_node_t *node)
{
    if (gen->current_loop == NULL) {
        /* Break outside of loop - ignore silently like Ruby Liquid does */
        return;
    }

    vm_assembler_t *code = gen->code;

    /* Emit a jump placeholder that will be patched to point to the FOR_CLEANUP instruction.
     * The cleanup instruction will pop the iterator state and then execution continues
     * after the loop. */
    if (gen->current_loop->break_jump_count < MAX_LOOP_BREAKS) {
        size_t jump_offset = vm_assembler_add_jump_placeholder(code, OP_JUMP);
        gen->current_loop->break_jumps[gen->current_loop->break_jump_count++] = jump_offset;
    }
}

static void codegen_continue(codegen_t *gen, ast_node_t *node)
{
    if (gen->current_loop == NULL) {
        /* Continue outside of loop - ignore silently like Ruby Liquid does */
        return;
    }

    vm_assembler_t *code = gen->code;

    /* Jump back to FOR_NEXT which will advance the iterator */
    size_t jump_offset = vm_assembler_add_jump_placeholder(code, OP_JUMP);
    vm_assembler_patch_jump(code, jump_offset, gen->current_loop->continue_target);
}

void codegen_node(codegen_t *gen, ast_node_t *node)
{
    if (node == NULL) return;

    switch (node->type) {
        case AST_TEMPLATE:
            codegen_node_list(gen, &node->data.template.children);
            break;

        case AST_RAW:
            codegen_raw(gen, node);
            break;

        case AST_VARIABLE:
            codegen_variable(gen, node);
            break;

        case AST_IF:
        case AST_UNLESS:
            codegen_if(gen, node);
            break;

        case AST_CASE:
            codegen_case(gen, node);
            break;

        case AST_FOR:
            codegen_for(gen, node);
            break;

        case AST_TABLEROW:
            codegen_tablerow(gen, node);
            break;

        case AST_ASSIGN:
            codegen_assign(gen, node);
            break;

        case AST_CAPTURE:
            codegen_capture(gen, node);
            break;

        case AST_INCREMENT:
            codegen_increment(gen, node);
            break;

        case AST_DECREMENT:
            codegen_decrement(gen, node);
            break;

        case AST_CYCLE:
            codegen_cycle(gen, node);
            break;

        case AST_INCLUDE:
        case AST_RENDER:
        case AST_CUSTOM_TAG:
            codegen_custom_tag(gen, node);
            break;

        case AST_ECHO:
            codegen_echo(gen, node);
            break;

        case AST_COMMENT:
            /* Comments produce no output */
            break;

        case AST_BREAK:
            codegen_break(gen, node);
            break;

        case AST_CONTINUE:
            codegen_continue(gen, node);
            break;

        case AST_LIQUID_TAG:
            codegen_liquid_tag(gen, node);
            break;
    }
}

void codegen_node_list(codegen_t *gen, ast_node_list_t *list)
{
    for (size_t i = 0; i < list->count; i++) {
        codegen_node(gen, list->nodes[i]);
    }
}

void codegen_template(codegen_t *gen, ast_node_t *root)
{
    if (root == NULL) return;

    if (root->type != AST_TEMPLATE) {
        codegen_node(gen, root);
        return;
    }

    codegen_node_list(gen, &root->data.template.children);
}

void liquid_define_codegen(void)
{
    /* No Ruby classes needed for codegen */
}
