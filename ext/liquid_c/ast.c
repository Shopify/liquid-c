#include "ast.h"
#include <string.h>

#define AST_NODE_LIST_INITIAL_CAPACITY 8

void ast_node_list_init(ast_node_list_t *list)
{
    list->nodes = NULL;
    list->count = 0;
    list->capacity = 0;
}

void ast_node_list_append(ast_node_list_t *list, ast_node_t *node, arena_t *arena)
{
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity == 0
            ? AST_NODE_LIST_INITIAL_CAPACITY
            : list->capacity * 2;

        ast_node_t **new_nodes = arena_alloc(arena, new_capacity * sizeof(ast_node_t *));

        if (list->nodes != NULL) {
            memcpy(new_nodes, list->nodes, list->count * sizeof(ast_node_t *));
        }

        list->nodes = new_nodes;
        list->capacity = new_capacity;
    }

    list->nodes[list->count++] = node;
}

ast_node_t *ast_node_alloc(arena_t *arena, ast_node_type_t type, unsigned int line_number)
{
    ast_node_t *node = arena_calloc(arena, 1, sizeof(ast_node_t));
    node->type = type;
    node->line_number = line_number;
    return node;
}

ast_condition_t *ast_condition_alloc(arena_t *arena)
{
    ast_condition_t *cond = arena_calloc(arena, 1, sizeof(ast_condition_t));
    return cond;
}

ast_branch_t *ast_branch_alloc(arena_t *arena)
{
    ast_branch_t *branch = arena_calloc(arena, 1, sizeof(ast_branch_t));
    ast_node_list_init(&branch->body);
    return branch;
}

void ast_init_assembler(vm_assembler_t *assembler)
{
    vm_assembler_init(assembler);
}

void ast_free_assembler(vm_assembler_t *assembler)
{
    vm_assembler_free(assembler);
}

static void ast_gc_mark_assembler(vm_assembler_t *assembler)
{
    if (assembler->constants_table != NULL) {
        vm_assembler_gc_mark(assembler);
    }
}

void ast_condition_gc_mark(ast_condition_t *condition)
{
    while (condition != NULL) {
        ast_gc_mark_assembler(&condition->left_expr);
        if (condition->comparison_op != CMP_NONE) {
            ast_gc_mark_assembler(&condition->right_expr);
        }
        condition = condition->next;
    }
}

void ast_branch_gc_mark(ast_branch_t *branch)
{
    while (branch != NULL) {
        if (branch->condition != NULL) {
            ast_condition_gc_mark(branch->condition);
        }
        ast_node_list_gc_mark(&branch->body);
        branch = branch->next;
    }
}

void ast_node_list_gc_mark(ast_node_list_t *list)
{
    for (size_t i = 0; i < list->count; i++) {
        ast_gc_mark(list->nodes[i]);
    }
}

void ast_gc_mark(ast_node_t *node)
{
    if (node == NULL) return;

    switch (node->type) {
        case AST_TEMPLATE:
            ast_node_list_gc_mark(&node->data.template.children);
            break;

        case AST_RAW:
            /* No Ruby objects */
            break;

        case AST_VARIABLE:
            ast_gc_mark_assembler(&node->data.variable.expr);
            break;

        case AST_IF:
        case AST_UNLESS:
            ast_branch_gc_mark(node->data.conditional.branches);
            break;

        case AST_CASE:
            ast_gc_mark_assembler(&node->data.case_stmt.target_expr);
            ast_branch_gc_mark(node->data.case_stmt.branches);
            break;

        case AST_FOR:
            rb_gc_mark(node->data.for_loop.var_name);
            ast_gc_mark_assembler(&node->data.for_loop.collection);
            if (node->data.for_loop.params.has_limit) {
                ast_gc_mark_assembler(&node->data.for_loop.params.limit_expr);
            }
            if (node->data.for_loop.params.has_offset) {
                ast_gc_mark_assembler(&node->data.for_loop.params.offset_expr);
            }
            ast_node_list_gc_mark(&node->data.for_loop.body);
            if (node->data.for_loop.has_else) {
                ast_node_list_gc_mark(&node->data.for_loop.else_body);
            }
            break;

        case AST_TABLEROW:
            rb_gc_mark(node->data.tablerow.var_name);
            ast_gc_mark_assembler(&node->data.tablerow.collection);
            if (node->data.tablerow.params.has_limit) {
                ast_gc_mark_assembler(&node->data.tablerow.params.limit_expr);
            }
            if (node->data.tablerow.params.has_offset) {
                ast_gc_mark_assembler(&node->data.tablerow.params.offset_expr);
            }
            if (node->data.tablerow.has_cols) {
                ast_gc_mark_assembler(&node->data.tablerow.cols_expr);
            }
            ast_node_list_gc_mark(&node->data.tablerow.body);
            break;

        case AST_ASSIGN:
            rb_gc_mark(node->data.assign.var_name);
            ast_gc_mark_assembler(&node->data.assign.expr);
            break;

        case AST_CAPTURE:
            rb_gc_mark(node->data.capture.var_name);
            ast_node_list_gc_mark(&node->data.capture.body);
            break;

        case AST_INCREMENT:
        case AST_DECREMENT:
            rb_gc_mark(node->data.counter.var_name);
            break;

        case AST_CYCLE:
            rb_gc_mark(node->data.cycle.group_name);
            for (size_t i = 0; i < node->data.cycle.value_count; i++) {
                ast_gc_mark_assembler(&node->data.cycle.values[i]);
            }
            break;

        case AST_INCLUDE:
        case AST_RENDER:
            ast_gc_mark_assembler(&node->data.include.template_expr);
            rb_gc_mark(node->data.include.variable_name);
            if (node->data.include.variable_name != Qnil) {
                ast_gc_mark_assembler(&node->data.include.variable_expr);
            }
            for (size_t i = 0; i < node->data.include.param_count; i++) {
                rb_gc_mark(node->data.include.param_names[i]);
                ast_gc_mark_assembler(&node->data.include.param_exprs[i]);
            }
            break;

        case AST_ECHO:
            ast_gc_mark_assembler(&node->data.echo.expr);
            break;

        case AST_COMMENT:
        case AST_BREAK:
        case AST_CONTINUE:
            /* No Ruby objects */
            break;

        case AST_CUSTOM_TAG:
            rb_gc_mark(node->data.custom_tag.tag_name);
            rb_gc_mark(node->data.custom_tag.markup);
            rb_gc_mark(node->data.custom_tag.tag_obj);
            break;

        case AST_LIQUID_TAG:
            ast_node_list_gc_mark(&node->data.liquid_tag.statements);
            break;
    }
}

const char *ast_node_type_name(ast_node_type_t type)
{
    switch (type) {
        case AST_TEMPLATE:    return "template";
        case AST_RAW:         return "raw";
        case AST_VARIABLE:    return "variable";
        case AST_IF:          return "if";
        case AST_UNLESS:      return "unless";
        case AST_CASE:        return "case";
        case AST_FOR:         return "for";
        case AST_TABLEROW:    return "tablerow";
        case AST_ASSIGN:      return "assign";
        case AST_CAPTURE:     return "capture";
        case AST_INCREMENT:   return "increment";
        case AST_DECREMENT:   return "decrement";
        case AST_CYCLE:       return "cycle";
        case AST_INCLUDE:     return "include";
        case AST_RENDER:      return "render";
        case AST_ECHO:        return "echo";
        case AST_COMMENT:     return "comment";
        case AST_BREAK:       return "break";
        case AST_CONTINUE:    return "continue";
        case AST_CUSTOM_TAG:  return "custom_tag";
        case AST_LIQUID_TAG:  return "liquid";
        default:              return "unknown";
    }
}
