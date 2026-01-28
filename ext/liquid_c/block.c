#include "liquid.h"
#include "block.h"
#include "intutil.h"
#include "tokenizer.h"
#include "stringutil.h"
#include "liquid_vm.h"
#include "variable.h"
#include "context.h"
#include "parse_context.h"
#include "vm_assembler.h"
#include "template_parser.h"
#include "codegen.h"
#include "ast.h"
#include "arena.h"
#include <stdio.h>

static ID
    intern_raise_missing_variable_terminator,
    intern_raise_missing_tag_terminator,
    intern_is_blank,
    intern_parse,
    intern_square_brackets,
    intern_unknown_tag_in_liquid_tag,
    intern_ivar_nodelist;

static VALUE tag_registry;
static VALUE variable_placeholder = Qnil;

typedef struct tag_markup {
    VALUE name;
    VALUE markup;
} tag_markup_t;

typedef struct parse_context {
    tokenizer_t *tokenizer;
    VALUE tokenizer_obj;
    VALUE ruby_obj;
} parse_context_t;

static void ensure_body_compiled(const block_body_t *body)
{
    if (!body->compiled) {
        rb_raise(rb_eRuntimeError, "Liquid::C::BlockBody has not been compiled");
    }
}

static void block_body_mark(void *ptr)
{
    block_body_t *body = ptr;
    if (body->compiled) {
        document_body_entry_mark(&body->as.compiled.document_body_entry);
        rb_gc_mark(body->as.compiled.nodelist);
    } else {
        rb_gc_mark(body->as.intermediate.parse_context);
        if (body->as.intermediate.vm_assembler_pool)
            rb_gc_mark(body->as.intermediate.vm_assembler_pool->self);
        if (body->as.intermediate.code)
            vm_assembler_gc_mark(body->as.intermediate.code);
    }
}

static void block_body_free(void *ptr)
{
    block_body_t *body = ptr;
    if (!body->compiled && body->as.intermediate.code) {
        // Free the assembler instead of recycling it because the vm_assembler_pool may have been GC'd
        vm_assembler_pool_free_assembler(body->as.intermediate.code);
    }
    xfree(body);
}

static size_t block_body_memsize(const void *ptr)
{
    const block_body_t *body = ptr;
    if (!ptr) return 0;
    if (body->compiled) {
        return sizeof(block_body_t);
    } else {
        return sizeof(block_body_t) + vm_assembler_alloc_memsize(body->as.intermediate.code);
    }
}

const rb_data_type_t block_body_data_type = {
    "liquid_block_body",
    { block_body_mark, block_body_free, block_body_memsize, },
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
};

#define BlockBody_Get_Struct(obj, sval) TypedData_Get_Struct(obj, block_body_t, &block_body_data_type, sval)

static VALUE block_body_allocate(VALUE klass)
{
    block_body_t *body;
    VALUE obj = TypedData_Make_Struct(klass, block_body_t, &block_body_data_type, body);

    body->compiled = false;
    body->obj = obj;
    body->as.intermediate.blank = true;
    body->as.intermediate.render_score = 0;
    body->as.intermediate.vm_assembler_pool = NULL;
    body->as.intermediate.code = NULL;
    return obj;
}

static VALUE block_body_initialize(VALUE self, VALUE parse_context)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);

    body->as.intermediate.parse_context = parse_context;
    body->as.intermediate.vm_assembler_pool = parse_context_get_vm_assembler_pool(parse_context);
    body->as.intermediate.code = vm_assembler_pool_alloc_assembler(body->as.intermediate.vm_assembler_pool);
    vm_assembler_add_leave(body->as.intermediate.code);

    return Qnil;
}

static int is_id(int c)
{
    return rb_isalnum(c) || c == '_';
}

/* Parse increment/decrement tag natively and emit OP_INCREMENT/OP_DECREMENT */
static bool parse_native_counter(block_body_t *body, const char *markup, const char *markup_end, bool is_increment)
{
    vm_assembler_t *code = body->as.intermediate.code;

    const char *cur = read_while(markup, markup_end, rb_isspace);

    /* Get variable name */
    const char *var_start = cur;
    while (cur < markup_end && is_id(*cur)) cur++;

    if (var_start == cur) return false;

    VALUE var_name = rb_enc_str_new(var_start, cur - var_start, utf8_encoding);

    if (is_increment) {
        vm_assembler_add_increment(code, var_name);
    } else {
        vm_assembler_add_decrement(code, var_name);
    }

    body->as.intermediate.blank = false;
    return true;
}

/* Check if markup contains patterns that would require Ruby fallback:
 * - 'and' or 'or' keywords (complex short-circuit evaluation)
 * - Potentially invalid operators like === (let Ruby handle lax mode errors)
 */
static bool markup_needs_ruby_fallback(const char *markup, const char *markup_end)
{
    const char *p = markup;
    bool in_string = false;
    char string_char = 0;

    while (p < markup_end) {
        char c = *p;

        if (in_string) {
            if (c == string_char) in_string = false;
            p++;
            continue;
        }

        if (c == '"' || c == '\'') {
            in_string = true;
            string_char = c;
            p++;
            continue;
        }

        /* Check for ' and ' or ' or ' */
        if (markup_end - p >= 5 && memcmp(p, " and ", 5) == 0) {
            return true;
        }
        if (markup_end - p >= 4 && memcmp(p, " or ", 4) == 0) {
            return true;
        }

        /* Check for potentially invalid operators (=== or similar) */
        /* Valid: ==, !=, <=, >=, <>, <, >
         * Invalid: ===, !==, etc. */
        if (c == '=' && markup_end - p >= 3) {
            if (p[1] == '=' && p[2] == '=') {
                return true;  /* === is invalid */
            }
        }

        p++;
    }
    return false;
}

/* Check if control flow block contains for loops which aren't fully implemented yet */
static bool block_contains_for_loop(parse_context_t *parse_context, const char *end_tag)
{
    tokenizer_t saved = *parse_context->tokenizer;

    token_t token;
    int depth = 1;
    bool has_for = false;

    while (depth > 0) {
        tokenizer_next(parse_context->tokenizer, &token);
        if (token.type == TOKENIZER_TOKEN_NONE) break;
        if (token.type != TOKEN_TAG) continue;

        const char *tag_start = token.str_trimmed;
        const char *tag_end = tag_start + token.len_trimmed;
        const char *name_start = read_while(tag_start, tag_end, rb_isspace);
        const char *name_end = read_while(name_start, tag_end, is_id);
        size_t name_len = name_end - name_start;

        if (name_len == 3 && strncmp(name_start, "for", 3) == 0) {
            has_for = true;
            break;
        }
        if (name_len == strlen(end_tag) && strncmp(name_start, end_tag, name_len) == 0) {
            depth--;
        }
        /* Track nested control flow */
        if (name_len == 2 && strncmp(name_start, "if", 2) == 0) depth++;
        if (name_len == 5 && strncmp(name_start, "endif", 5) == 0) depth--;
        if (name_len == 6 && strncmp(name_start, "unless", 6) == 0) depth++;
        if (name_len == 9 && strncmp(name_start, "endunless", 9) == 0) depth--;
        if (name_len == 4 && strncmp(name_start, "case", 4) == 0) depth++;
        if (name_len == 7 && strncmp(name_start, "endcase", 7) == 0) depth--;
    }

    *parse_context->tokenizer = saved;
    return has_for;
}

/* Check if control flow block contains break/continue tags (fallback to Ruby for interrupts) */
static bool block_contains_interrupt_tag(parse_context_t *parse_context, const char *end_tag)
{
    tokenizer_t saved = *parse_context->tokenizer;

    token_t token;
    int depth = 1;
    bool has_interrupt = false;

    while (depth > 0) {
        tokenizer_next(parse_context->tokenizer, &token);
        if (token.type == TOKENIZER_TOKEN_NONE) break;
        if (token.type != TOKEN_TAG) continue;

        const char *tag_start = token.str_trimmed;
        const char *tag_end = tag_start + token.len_trimmed;
        const char *name_start = read_while(tag_start, tag_end, rb_isspace);
        const char *name_end = read_while(name_start, tag_end, is_id);
        size_t name_len = name_end - name_start;

        if ((name_len == 5 && strncmp(name_start, "break", 5) == 0) ||
            (name_len == 8 && strncmp(name_start, "continue", 8) == 0)) {
            has_interrupt = true;
            break;
        }
        if (name_len == strlen(end_tag) && strncmp(name_start, end_tag, name_len) == 0) {
            depth--;
        }
        /* Track nested control flow */
        if (name_len == 2 && strncmp(name_start, "if", 2) == 0) depth++;
        if (name_len == 5 && strncmp(name_start, "endif", 5) == 0) depth--;
        if (name_len == 6 && strncmp(name_start, "unless", 6) == 0) depth++;
        if (name_len == 9 && strncmp(name_start, "endunless", 9) == 0) depth--;
        if (name_len == 4 && strncmp(name_start, "case", 4) == 0) depth++;
        if (name_len == 7 && strncmp(name_start, "endcase", 7) == 0) depth--;
    }

    *parse_context->tokenizer = saved;
    return has_interrupt;
}

/* Check if case statement has multiple values in when clauses (comma-separated) */
static bool case_has_multiple_when_values(parse_context_t *parse_context)
{
    /* Look ahead to see if any when clause has commas
     * This is a heuristic - we don't fully parse, just scan for when...comma patterns */
    tokenizer_t saved = *parse_context->tokenizer;

    token_t token;
    int depth = 1;  /* Track nesting of case statements */
    bool has_multiple = false;

    while (depth > 0) {
        tokenizer_next(parse_context->tokenizer, &token);
        if (token.type == TOKENIZER_TOKEN_NONE) break;
        if (token.type != TOKEN_TAG) continue;

        const char *tag_start = token.str_trimmed;
        const char *tag_end = tag_start + token.len_trimmed;
        const char *name_start = read_while(tag_start, tag_end, rb_isspace);
        const char *name_end = read_while(name_start, tag_end, is_id);
        size_t name_len = name_end - name_start;

        if (name_len == 4 && strncmp(name_start, "case", 4) == 0) {
            depth++;
        } else if (name_len == 7 && strncmp(name_start, "endcase", 7) == 0) {
            depth--;
        } else if (depth == 1 && name_len == 4 && strncmp(name_start, "when", 4) == 0) {
            /* Check if there's a comma in the when markup (outside strings) */
            const char *markup = read_while(name_end, tag_end, rb_isspace);
            bool in_string = false;
            char string_char = 0;
            const char *p = markup;
            while (p < tag_end) {
                char c = *p;
                if (in_string) {
                    if (c == string_char) in_string = false;
                } else {
                    if (c == '"' || c == '\'') {
                        in_string = true;
                        string_char = c;
                    } else if (c == ',') {
                        has_multiple = true;
                        break;
                    }
                }
                p++;
            }
            if (has_multiple) break;
        }
    }

    /* Restore tokenizer state */
    *parse_context->tokenizer = saved;
    return has_multiple;
}

/*
 * Parse a control flow structure (if/unless/case) using template_parser
 * and emit native bytecode using codegen.
 *
 * This function:
 * 1. Creates a template_parser and parses the full control flow structure
 * 2. Uses codegen to emit native jump/comparison opcodes
 * 3. Updates the body's blank and render_score tracking
 *
 * Returns true if successfully parsed, false if should fall back to Ruby.
 */
static bool parse_native_control_flow(block_body_t *body, parse_context_t *parse_context,
                                       token_t *token, const char *tag_name, size_t tag_len,
                                       const char *markup, const char *markup_end)
{
    vm_assembler_t *code = body->as.intermediate.code;

    /* Skip native parsing for conditions with 'and'/'or' or invalid operators */
    if ((tag_len == 2 && strncmp(tag_name, "if", 2) == 0) ||
        (tag_len == 6 && strncmp(tag_name, "unless", 6) == 0)) {
        if (markup_needs_ruby_fallback(markup, markup_end)) {
            return false;
        }
        /* Check for empty condition - let Ruby handle the error */
        const char *p = read_while(markup, markup_end, rb_isspace);
        if (p >= markup_end) {
            return false;
        }
        /* Skip if block contains for loops (not fully implemented) */
        const char *end_tag = (tag_len == 2) ? "endif" : "endunless";
        if (block_contains_for_loop(parse_context, end_tag)) {
            return false;
        }
        if (block_contains_interrupt_tag(parse_context, end_tag)) {
            return false;
        }
    }

    /* Skip native parsing for case statements with multiple when values or containing for loops */
    if (tag_len == 4 && strncmp(tag_name, "case", 4) == 0) {
        if (case_has_multiple_when_values(parse_context)) {
            return false;
        }
        if (block_contains_for_loop(parse_context, "endcase")) {
            return false;
        }
        if (block_contains_interrupt_tag(parse_context, "endcase")) {
            return false;
        }
    }

    /* Initialize template parser */
    template_parser_t parser;
    template_parser_init(&parser, parse_context->tokenizer_obj, parse_context->ruby_obj);
    VALUE parser_guard = template_parser_gc_guard_new(&parser);
    rb_gc_register_address(&parser_guard);
    bool ok = false;

    /* Parse the control flow tag into AST */
    ast_node_t *ast = NULL;

    /* Set up error handling */
    if (setjmp(parser.error_jmp)) {
        /* Parse error - fall back to Ruby */
        goto cleanup;
    }

    /* Parse based on tag type */
    if (tag_len == 2 && strncmp(tag_name, "if", 2) == 0) {
        ast = ast_node_alloc(&parser.arena, AST_IF, parse_context->tokenizer->line_number);
        parser.root = ast;

        /* Parse initial condition */
        ast_branch_t *first_branch = ast_branch_alloc(&parser.arena);
        first_branch->condition = template_parser_parse_condition(&parser, markup, markup_end);
        ast_node_list_init(&first_branch->body);

        ast->data.conditional.branches = first_branch;
        ast_branch_t *last_branch = first_branch;

        /* Parse body until elsif/else/endif */
        const char *end_tags[] = { "elsif", "else", "endif" };
        VALUE end_tag;

        while (true) {
            end_tag = template_parser_parse_body(&parser, &last_branch->body, end_tags, 3);

            if (end_tag == Qnil) {
                goto cleanup; /* Unclosed tag - let Ruby handle the error */
            }

            const char *end_name = RSTRING_PTR(end_tag);
            size_t end_len = RSTRING_LEN(end_tag);

            if (end_len == 5 && strncmp(end_name, "endif", 5) == 0) {
                break;
            } else if (end_len == 5 && strncmp(end_name, "elsif", 5) == 0) {
                /* Get elsif condition from the current token */
                const char *elsif_markup = parser.current_token.str_trimmed;
                const char *elsif_end = elsif_markup + parser.current_token.len_trimmed;

                /* Skip "elsif" keyword and whitespace */
                elsif_markup = read_while(elsif_markup, elsif_end, rb_isspace);
                elsif_markup += 5;
                elsif_markup = read_while(elsif_markup, elsif_end, rb_isspace);

                ast_branch_t *elsif_branch = ast_branch_alloc(&parser.arena);
                elsif_branch->condition = template_parser_parse_condition(&parser, elsif_markup, elsif_end);
                ast_node_list_init(&elsif_branch->body);

                last_branch->next = elsif_branch;
                last_branch = elsif_branch;
            } else if (end_len == 4 && strncmp(end_name, "else", 4) == 0) {
                ast_branch_t *else_branch = ast_branch_alloc(&parser.arena);
                else_branch->condition = NULL;
                ast_node_list_init(&else_branch->body);

                last_branch->next = else_branch;
                last_branch = else_branch;

                /* Parse until endif */
                const char *final_tags[] = { "endif" };
                end_tag = template_parser_parse_body(&parser, &last_branch->body, final_tags, 1);

                if (end_tag == Qnil) {
                    goto cleanup;
                }
                break;
            }
        }
    } else if (tag_len == 6 && strncmp(tag_name, "unless", 6) == 0) {
        ast = ast_node_alloc(&parser.arena, AST_UNLESS, parse_context->tokenizer->line_number);
        parser.root = ast;

        ast_branch_t *first_branch = ast_branch_alloc(&parser.arena);
        first_branch->condition = template_parser_parse_condition(&parser, markup, markup_end);
        ast_node_list_init(&first_branch->body);

        ast->data.conditional.branches = first_branch;
        ast_branch_t *last_branch = first_branch;

        const char *end_tags[] = { "else", "endunless" };
        VALUE end_tag;

        while (true) {
            end_tag = template_parser_parse_body(&parser, &last_branch->body, end_tags, 2);

            if (end_tag == Qnil) {
                goto cleanup;
            }

            const char *end_name = RSTRING_PTR(end_tag);
            size_t end_len = RSTRING_LEN(end_tag);

            if (end_len == 9 && strncmp(end_name, "endunless", 9) == 0) {
                break;
            } else if (end_len == 4 && strncmp(end_name, "else", 4) == 0) {
                ast_branch_t *else_branch = ast_branch_alloc(&parser.arena);
                else_branch->condition = NULL;
                ast_node_list_init(&else_branch->body);

                last_branch->next = else_branch;
                last_branch = else_branch;

                const char *final_tags[] = { "endunless" };
                end_tag = template_parser_parse_body(&parser, &last_branch->body, final_tags, 1);

                if (end_tag == Qnil) {
                    goto cleanup;
                }
                break;
            }
        }
    } else if (tag_len == 4 && strncmp(tag_name, "case", 4) == 0) {
        ast = ast_node_alloc(&parser.arena, AST_CASE, parse_context->tokenizer->line_number);
        parser.root = ast;

        /* Parse target expression */
        ast_init_assembler(&ast->data.case_stmt.target_expr);
        template_parser_parse_expression(&parser, markup, markup_end, &ast->data.case_stmt.target_expr);

        ast->data.case_stmt.branches = NULL;
        ast_branch_t *last_branch = NULL;

        const char *end_tags[] = { "when", "else", "endcase" };
        VALUE end_tag;

        while (true) {
            ast_node_list_t *body_list = NULL;
            if (last_branch != NULL) {
                body_list = &last_branch->body;
            } else {
                /* Allocate a temporary list for content before first when */
                static ast_node_list_t dummy;
                ast_node_list_init(&dummy);
                body_list = &dummy;
            }

            end_tag = template_parser_parse_body(&parser, body_list, end_tags, 3);

            if (end_tag == Qnil) {
                goto cleanup;
            }

            const char *end_name = RSTRING_PTR(end_tag);
            size_t end_len = RSTRING_LEN(end_tag);

            if (end_len == 7 && strncmp(end_name, "endcase", 7) == 0) {
                break;
            } else if (end_len == 4 && strncmp(end_name, "when", 4) == 0) {
                /* Get when values from current token */
                const char *when_markup = parser.current_token.str_trimmed;
                const char *when_end = when_markup + parser.current_token.len_trimmed;

                when_markup = read_while(when_markup, when_end, rb_isspace);
                when_markup += 4;
                when_markup = read_while(when_markup, when_end, rb_isspace);

                ast_branch_t *when_branch = ast_branch_alloc(&parser.arena);
                when_branch->condition = ast_condition_alloc(&parser.arena);
                ast_init_assembler(&when_branch->condition->left_expr);
                template_parser_parse_expression(&parser, when_markup, when_end, &when_branch->condition->left_expr);
                ast_node_list_init(&when_branch->body);

                if (last_branch != NULL) {
                    last_branch->next = when_branch;
                } else {
                    ast->data.case_stmt.branches = when_branch;
                }
                last_branch = when_branch;
            } else if (end_len == 4 && strncmp(end_name, "else", 4) == 0) {
                ast_branch_t *else_branch = ast_branch_alloc(&parser.arena);
                else_branch->condition = NULL;
                ast_node_list_init(&else_branch->body);

                if (last_branch != NULL) {
                    last_branch->next = else_branch;
                } else {
                    ast->data.case_stmt.branches = else_branch;
                }
                last_branch = else_branch;

                const char *final_tags[] = { "endcase" };
                end_tag = template_parser_parse_body(&parser, &last_branch->body, final_tags, 1);

                if (end_tag == Qnil) {
                    goto cleanup;
                }
                break;
            }
        }
    } else {
        goto cleanup;
    }

    if (ast == NULL) {
        goto cleanup;
    }

    /* Generate bytecode from AST */
    codegen_t gen;
    codegen_init(&gen, code, body->obj, &parser.arena);
    codegen_node(&gen, ast);

    /* Update body tracking */
    body->as.intermediate.render_score += gen.render_score;
    if (!gen.is_blank) {
        body->as.intermediate.blank = false;
    }

    ok = true;

cleanup:
    /* Free parser resources */
    template_parser_free(&parser);
    rb_gc_unregister_address(&parser_guard);
    RB_GC_GUARD(parser_guard);

    return ok;
}


static tag_markup_t internal_block_body_parse(block_body_t *body, parse_context_t *parse_context)
{
    tokenizer_t *tokenizer = parse_context->tokenizer;
    token_t token;
    tag_markup_t unknown_tag = { Qnil, Qnil };
    int render_score_increment = 0;

    while (true) {
        int token_start_line_number = tokenizer->line_number;
        if (token_start_line_number != 0) {
            rb_ivar_set(parse_context->ruby_obj, id_ivar_line_number, UINT2NUM(token_start_line_number));
        }
        tokenizer_next(tokenizer, &token);

        switch (token.type) {
            case TOKENIZER_TOKEN_NONE:
                goto loop_break;

            case TOKEN_INVALID:
            {
                VALUE str = rb_enc_str_new(token.str_full, token.len_full, utf8_encoding);

                ID raise_method_id = intern_raise_missing_variable_terminator;
                if (token.str_full[1] == '%') raise_method_id = intern_raise_missing_tag_terminator;

                rb_funcall(cLiquidBlockBody, raise_method_id, 2, str, parse_context->ruby_obj);
                goto loop_break;
            }
            case TOKEN_RAW:
            {
                const char *start = token.str_full, *end = token.str_full + token.len_full;
                const char *token_start = start, *token_end = end;

                if (token.lstrip)
                    token_start = read_while(start, end, rb_isspace);

                if (token.rstrip) {
                    if (tokenizer->bug_compatible_whitespace_trimming) {
                        token_end = read_while_reverse(token_start + 1, end, rb_isspace);
                    } else {
                        token_end = read_while_reverse(token_start, end, rb_isspace);
                    }
                }

                // Skip token entirely if there is no data to be rendered.
                if (token_start == token_end)
                    break;

                vm_assembler_add_write_raw(body->as.intermediate.code, token_start, token_end - token_start);
                render_score_increment += 1;

                if (body->as.intermediate.blank) {
                    const char *end = token.str_full + token.len_full;

                    if (read_while(token.str_full, end, rb_isspace) < end)
                        body->as.intermediate.blank = false;
                }
                break;
            }
            case TOKEN_VARIABLE:
            {
                variable_parse_args_t parse_args = {
                    .markup = token.str_trimmed,
                    .markup_end = token.str_trimmed + token.len_trimmed,
                    .code = body->as.intermediate.code,
                    .code_obj = body->obj,
                    .parse_context = parse_context->ruby_obj,
                };
                internal_variable_compile(&parse_args, token_start_line_number);
                render_score_increment += 1;
                body->as.intermediate.blank = false;
                break;
            }
            case TOKEN_TAG:
            {
                const char *start = token.str_trimmed, *end = token.str_trimmed + token.len_trimmed;

                // Imitate \s*(\w+)\s*(.*)? regex
                const char *name_start = read_while(start, end, rb_isspace);
                const char *name_end = read_while(name_start, end, is_id);
                long name_len = name_end - name_start;

                if (name_len == 0) {
                    if (name_start < end && *name_start == '#') { // inline comment
                        name_end++;
                        name_len++;
                    } else {
                        VALUE str = rb_enc_str_new(token.str_trimmed, token.len_trimmed, utf8_encoding);
                        unknown_tag = (tag_markup_t) { str, str };
                        goto loop_break;
                    }
                }

                if (name_len == 6 && strncmp(name_start, "liquid", 6) == 0) {
                    const char *markup_start = read_while(name_end, end, rb_isspace);
                    int line_number = token_start_line_number;
                    if (line_number) {
                        line_number += count_newlines(token.str_full, markup_start);
                    }

                    tokenizer_t saved_tokenizer = *tokenizer;
                    tokenizer_setup_for_liquid_tag(tokenizer, markup_start, end, line_number);
                    unknown_tag = internal_block_body_parse(body, parse_context);
                    *tokenizer = saved_tokenizer;
                    if (unknown_tag.name != Qnil) {
                        rb_funcall(cLiquidBlockBody, intern_unknown_tag_in_liquid_tag, 2, unknown_tag.name, parse_context->ruby_obj);
                        goto loop_break;
                    }
                    break;
                }

                const char *markup_start = read_while(name_end, end, rb_isspace);

                /* Try native parsing for performance-critical simple tags.
                 * These emit native opcodes directly, bypassing Ruby tag creation.
                 * nodelist reconstruction handles creating synthetic tag objects. */
                if (name_len == 9 && strncmp(name_start, "increment", 9) == 0) {
                    if (parse_native_counter(body, markup_start, end, true)) {
                        render_score_increment += 1;
                        break;
                    }
                    /* Fall through to Ruby parsing on failure */
                }
                if (name_len == 9 && strncmp(name_start, "decrement", 9) == 0) {
                    if (parse_native_counter(body, markup_start, end, false)) {
                        render_score_increment += 1;
                        break;
                    }
                    /* Fall through to Ruby parsing on failure */
                }

                /* Native control flow parsing for if/unless/case.
                 * These parse the entire block structure and emit native jump/comparison opcodes. */
                if ((name_len == 2 && strncmp(name_start, "if", 2) == 0) ||
                    (name_len == 6 && strncmp(name_start, "unless", 6) == 0) ||
                    (name_len == 4 && strncmp(name_start, "case", 4) == 0)) {
                    if (parse_native_control_flow(body, parse_context, &token, name_start, name_len, markup_start, end)) {
                        /* Successfully parsed native control flow - continue to next token */
                        break;
                    }
                    /* Fall through to Ruby parsing on failure */
                }

                VALUE tag_name = rb_enc_str_new(name_start, name_end - name_start, utf8_encoding);
                VALUE tag_class = rb_funcall(tag_registry, intern_square_brackets, 1, tag_name);

                VALUE markup = rb_enc_str_new(markup_start, end - markup_start, utf8_encoding);

                if (tag_class == Qnil) {
                    unknown_tag = (tag_markup_t) { tag_name, markup };
                    goto loop_break;
                }

                VALUE new_tag = rb_funcall(tag_class, intern_parse, 4,
                        tag_name, markup, parse_context->tokenizer_obj, parse_context->ruby_obj);

                if (body->as.intermediate.blank && !RTEST(rb_funcall(new_tag, intern_is_blank, 0)))
                    body->as.intermediate.blank = false;

                if (tokenizer->raw_tag_body) {
                    if (tokenizer->raw_tag_body_len) {
                        vm_assembler_add_write_raw(body->as.intermediate.code, tokenizer->raw_tag_body,
                                                tokenizer->raw_tag_body_len);
                    }
                    tokenizer->raw_tag_body = NULL;
                    tokenizer->raw_tag_body_len = 0;
                } else {
                    vm_assembler_add_write_node(body->as.intermediate.code, new_tag);
                }

                render_score_increment += 1;
                break;
            }
            case TOKEN_BLANK_LIQUID_TAG_LINE:
                break;
        }
    }
loop_break:
    body->as.intermediate.render_score += render_score_increment;
    return unknown_tag;
}

static void ensure_intermediate(block_body_t *body)
{
    if (body->compiled) {
        rb_raise(rb_eRuntimeError, "Liquid::C::BlockBody is already compiled");
    }
}

static void ensure_intermediate_not_parsing(block_body_t *body)
{
    ensure_intermediate(body);

    if (body->as.intermediate.code->parsing) {
        rb_raise(rb_eRuntimeError, "Liquid::C::BlockBody is in a incompletely parsed state");
    }
}

static VALUE block_body_parse(VALUE self, VALUE tokenizer_obj, VALUE parse_context_obj)
{
    parse_context_t parse_context = {
        .tokenizer_obj = tokenizer_obj,
        .ruby_obj = parse_context_obj,
    };
    Tokenizer_Get_Struct(tokenizer_obj, parse_context.tokenizer);
    block_body_t *body;
    BlockBody_Get_Struct(self, body);

    ensure_intermediate_not_parsing(body);
    if (body->as.intermediate.parse_context != parse_context_obj) {
        rb_raise(rb_eArgError, "Liquid::C::BlockBody#parse called with different parse context");
    }
    vm_assembler_remove_leave(body->as.intermediate.code); // to extend block

    tag_markup_t unknown_tag = internal_block_body_parse(body, &parse_context);
    vm_assembler_add_leave(body->as.intermediate.code);

    return rb_yield_values(2, unknown_tag.name, unknown_tag.markup);
}


/*
 * Parse the entire template using native template_parser + codegen.
 * This provides better performance by:
 * 1. Parsing the whole template into an AST in C
 * 2. Generating native bytecode for all supported tags
 * 3. Only falling back to Ruby for custom tags (AST_CUSTOM_TAG)
 *
 * Returns true if native parsing succeeded, false if should fall back to Ruby parsing.
 */
static VALUE block_body_parse_native(VALUE self, VALUE tokenizer_obj, VALUE parse_context_obj)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);

    ensure_intermediate_not_parsing(body);
    if (body->as.intermediate.parse_context != parse_context_obj) {
        rb_raise(rb_eArgError, "Liquid::C::BlockBody#parse_native called with different parse context");
    }

    parse_context_t parse_context = {
        .tokenizer_obj = tokenizer_obj,
        .ruby_obj = parse_context_obj,
    };
    Tokenizer_Get_Struct(tokenizer_obj, parse_context.tokenizer);

    /* Initialize template parser */
    template_parser_t parser;
    template_parser_init(&parser, tokenizer_obj, parse_context_obj);
    VALUE parser_guard = template_parser_gc_guard_new(&parser);
    rb_gc_register_address(&parser_guard);
    VALUE result = Qfalse;

    /* Parse entire template into AST */
    ast_node_t *ast = template_parser_parse(&parser);

    if (ast == NULL || parser.error_occurred) {
        /* Parse error - clean up and return false to fall back to Ruby */
        goto cleanup;
    }

    /* Check if AST contains any custom tags - if so, fall back to Ruby for now */
    /* TODO: Support mixed native/Ruby execution for templates with custom tags */

    /* Remove leave instruction to extend block */
    vm_assembler_remove_leave(body->as.intermediate.code);

    /* Generate bytecode from AST */
    codegen_t gen;
    codegen_init(&gen, body->as.intermediate.code, self, &parser.arena);
    codegen_node(&gen, ast);

    /* Update body tracking */
    body->as.intermediate.render_score += gen.render_score;
    if (!gen.is_blank) {
        body->as.intermediate.blank = false;
    }

    /* Add leave instruction */
    vm_assembler_add_leave(body->as.intermediate.code);

    result = Qtrue;

cleanup:
    /* Free parser resources */
    template_parser_free(&parser);
    rb_gc_unregister_address(&parser_guard);
    RB_GC_GUARD(parser_guard);
    return result;
}

static VALUE block_body_freeze(VALUE self)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);

    if (body->compiled) return Qnil;

    VALUE parse_context = body->as.intermediate.parse_context;
    VALUE document_body = parse_context_get_document_body(parse_context);
    rb_check_frozen(document_body);

    vm_assembler_pool_t *assembler_pool = body->as.intermediate.vm_assembler_pool;
    vm_assembler_t *assembler = body->as.intermediate.code;
    bool blank = body->as.intermediate.blank;
    uint32_t render_score = body->as.intermediate.render_score;
    vm_assembler_t *code = body->as.intermediate.code;
    body->as.compiled.document_body_entry = document_body_write_block_body(document_body, blank, render_score, code);
    body->as.compiled.nodelist = Qundef;
    body->compiled = true;
    vm_assembler_pool_recycle_assembler(assembler_pool, assembler);

    rb_call_super(0, NULL);

    return Qnil;
}

static VALUE block_body_render_to_output_buffer(VALUE self, VALUE context, VALUE output)
{
    Check_Type(output, T_STRING);
    check_utf8_encoding(output, "output");

    block_body_t *body;
    BlockBody_Get_Struct(self, body);
    ensure_body_compiled(body);
    document_body_entry_t *entry = &body->as.compiled.document_body_entry;
    document_body_ensure_compile_finished(entry->body);

    liquid_vm_render(document_body_get_block_body_header_ptr(entry), document_body_get_constants_ptr(entry), context, output);
    return output;
}

static VALUE block_body_blank_p(VALUE self)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);
    if (body->compiled) {
        block_body_header_t *body_header = document_body_get_block_body_header_ptr(&body->as.compiled.document_body_entry);
        return BLOCK_BODY_HEADER_BLANK_P(body_header) ? Qtrue : Qfalse;
    } else {
        return body->as.intermediate.blank ? Qtrue : Qfalse;
    }
}

static VALUE block_body_remove_blank_strings(VALUE self)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);

    ensure_intermediate_not_parsing(body);

    if (!body->as.intermediate.blank) {
        rb_raise(rb_eRuntimeError, "remove_blank_strings only support being called on a blank block body");
    }

    uint8_t *ip = body->as.intermediate.code->instructions.data;

    while (*ip != OP_LEAVE) {
        if (*ip == OP_WRITE_RAW) {
            if (ip[1]) { // if (size != 0)
                ip[0] = OP_JUMP_FWD; // effectively a no-op
                body->as.intermediate.render_score--;
            }
        } else if (*ip == OP_WRITE_RAW_W) {
            if (ip[1] || ip[2] || ip[3]) { // if (size != 0)
                ip[0] = OP_JUMP_FWD_W; // effectively a no-op
                body->as.intermediate.render_score--;
            }
        }
        liquid_vm_next_instruction((const uint8_t **)&ip);
    }

    return Qnil;
}

static void memoize_variable_placeholder(void)
{
    if (variable_placeholder == Qnil) {
        VALUE cLiquidCVariablePlaceholder = rb_const_get(mLiquidC, rb_intern("VariablePlaceholder"));
        variable_placeholder = rb_class_new_instance(0, NULL, cLiquidCVariablePlaceholder);
    }
}

// Cached Liquid tag classes for synthetic nodelist construction
static VALUE cLiquidIncrement = Qnil;
static VALUE cLiquidDecrement = Qnil;

// Deprecated: avoid using this for the love of performance
static VALUE block_body_nodelist(VALUE self)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);
    ensure_body_compiled(body);
    document_body_entry_t *entry = &body->as.compiled.document_body_entry;
    block_body_header_t *body_header = document_body_get_block_body_header_ptr(entry);

    memoize_variable_placeholder();

    if (body->as.compiled.nodelist != Qundef)
        return body->as.compiled.nodelist;

    VALUE nodelist = rb_ary_new_capa(body_header->render_score);

    const VALUE *constants = &entry->body->constants;
    const uint8_t *ip = block_body_instructions_ptr(body_header);
    while (true) {
        switch (*ip) {
            case OP_LEAVE:
                goto loop_break;
            case OP_WRITE_RAW_W:
            case OP_WRITE_RAW:
            {
                const char *text;
                size_t size;
                if (*ip == OP_WRITE_RAW_W) {
                    size = bytes_to_uint24(&ip[1]);
                    text = (const char *)&ip[4];
                } else {
                    size = ip[1];
                    text = (const char *)&ip[2];
                }
                VALUE string = rb_enc_str_new(text, size, utf8_encoding);
                rb_ary_push(nodelist, string);
                break;
            }
            case OP_WRITE_NODE:
            {
                uint16_t constant_index = (ip[1] << 8) | ip[2];
                VALUE node = RARRAY_AREF(*constants, constant_index);
                rb_ary_push(nodelist, node);
                break;
            }

            case OP_RENDER_VARIABLE_RESCUE:
                rb_ary_push(nodelist, variable_placeholder);
                break;

            /* Handle native opcodes - add variable name as placeholder for nodelist.
             * Full tag objects would require parse_context which we don't have here. */
            case OP_INCREMENT:
            case OP_DECREMENT:
            case OP_ASSIGN:
            {
                uint16_t constant_index = (ip[1] << 8) | ip[2];
                VALUE var_name = RARRAY_AREF(*constants, constant_index);
                /* Add the variable name as a placeholder - this preserves some
                 * debugging info while avoiding the complexity of synthesizing
                 * full tag objects */
                rb_ary_push(nodelist, var_name);
                break;
            }
        }
        liquid_vm_next_instruction(&ip);
    }
loop_break:

    rb_ary_freeze(nodelist);
    body->as.compiled.nodelist = nodelist;
    return nodelist;
}

static VALUE block_body_disassemble(VALUE self)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);
    document_body_entry_t *entry = &body->as.compiled.document_body_entry;
    block_body_header_t *header = document_body_get_block_body_header_ptr(entry);
    const uint8_t *start_ip = block_body_instructions_ptr(header);
    return vm_assembler_disassemble(
        start_ip,
        start_ip + header->instructions_bytes,
        &entry->body->constants
    );
}


static VALUE block_body_add_evaluate_expression(VALUE self, VALUE expression)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);
    ensure_intermediate(body);
    vm_assembler_add_evaluate_expression_from_ruby(body->as.intermediate.code, self, expression);
    return self;
}

static VALUE block_body_add_find_variable(VALUE self, VALUE expression)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);
    ensure_intermediate(body);
    vm_assembler_add_find_variable_from_ruby(body->as.intermediate.code, self, expression);
    return self;
}

static VALUE block_body_add_lookup_command(VALUE self, VALUE name)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);
    ensure_intermediate(body);
    vm_assembler_add_lookup_command_from_ruby(body->as.intermediate.code, name);
    return self;
}

static VALUE block_body_add_lookup_key(VALUE self, VALUE expression)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);
    ensure_intermediate(body);
    vm_assembler_add_lookup_key_from_ruby(body->as.intermediate.code, self, expression);
    return self;
}

static VALUE block_body_add_new_int_range(VALUE self)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);
    ensure_intermediate(body);
    vm_assembler_add_new_int_range_from_ruby(body->as.intermediate.code);
    return self;
}

static VALUE block_body_add_hash_new(VALUE self, VALUE hash_size)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);
    ensure_intermediate(body);
    vm_assembler_add_hash_new_from_ruby(body->as.intermediate.code, hash_size);
    return self;
}

static VALUE block_body_add_filter(VALUE self, VALUE filter_name, VALUE num_args)
{
    block_body_t *body;
    BlockBody_Get_Struct(self, body);
    ensure_intermediate(body);
    vm_assembler_add_filter_from_ruby(body->as.intermediate.code, filter_name, num_args);
    return self;
}


void liquid_define_block_body(void)
{
    intern_raise_missing_variable_terminator = rb_intern("raise_missing_variable_terminator");
    intern_raise_missing_tag_terminator = rb_intern("raise_missing_tag_terminator");
    intern_is_blank = rb_intern("blank?");
    intern_parse = rb_intern("parse");
    intern_square_brackets = rb_intern("[]");
    intern_unknown_tag_in_liquid_tag = rb_intern("unknown_tag_in_liquid_tag");
    intern_ivar_nodelist = rb_intern("@nodelist");

    tag_registry = rb_funcall(cLiquidTemplate, rb_intern("tags"), 0);
    rb_global_variable(&tag_registry);

    /* Cache tag classes for synthetic nodelist construction */
    if (rb_const_defined(mLiquid, rb_intern("Increment"))) {
        cLiquidIncrement = rb_const_get(mLiquid, rb_intern("Increment"));
        rb_global_variable(&cLiquidIncrement);
    }
    if (rb_const_defined(mLiquid, rb_intern("Decrement"))) {
        cLiquidDecrement = rb_const_get(mLiquid, rb_intern("Decrement"));
        rb_global_variable(&cLiquidDecrement);
    }

    VALUE cLiquidCBlockBody = rb_define_class_under(mLiquidC, "BlockBody", rb_cObject);
    rb_define_alloc_func(cLiquidCBlockBody, block_body_allocate);

    rb_define_method(cLiquidCBlockBody, "initialize", block_body_initialize, 1);
    rb_define_method(cLiquidCBlockBody, "parse", block_body_parse, 2);
    rb_define_method(cLiquidCBlockBody, "parse_native", block_body_parse_native, 2);
    rb_define_method(cLiquidCBlockBody, "freeze", block_body_freeze, 0);
    rb_define_method(cLiquidCBlockBody, "render_to_output_buffer", block_body_render_to_output_buffer, 2);
    rb_define_method(cLiquidCBlockBody, "remove_blank_strings", block_body_remove_blank_strings, 0);
    rb_define_method(cLiquidCBlockBody, "blank?", block_body_blank_p, 0);
    rb_define_method(cLiquidCBlockBody, "nodelist", block_body_nodelist, 0);
    rb_define_method(cLiquidCBlockBody, "disassemble", block_body_disassemble, 0);

    rb_define_method(cLiquidCBlockBody, "add_evaluate_expression", block_body_add_evaluate_expression, 1);
    rb_define_method(cLiquidCBlockBody, "add_find_variable", block_body_add_find_variable, 1);
    rb_define_method(cLiquidCBlockBody, "add_lookup_command", block_body_add_lookup_command, 1);
    rb_define_method(cLiquidCBlockBody, "add_lookup_key", block_body_add_lookup_key, 1);
    rb_define_method(cLiquidCBlockBody, "add_new_int_range", block_body_add_new_int_range, 0);

    rb_define_method(cLiquidCBlockBody, "add_hash_new", block_body_add_hash_new, 1);
    rb_define_method(cLiquidCBlockBody, "add_filter", block_body_add_filter, 2);

    rb_global_variable(&variable_placeholder);
}
