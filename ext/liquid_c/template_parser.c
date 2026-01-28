#include "template_parser.h"
#include "liquid.h"
#include "lexer.h"
#include "stringutil.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Intern IDs */
static ID intern_parse;
static ID intern_square_brackets;
static ID intern_tags;

/* Forward declarations */
static ast_node_t *parse_tag(template_parser_t *parser, token_t *token);
static ast_node_t *parse_if(template_parser_t *parser, const char *markup, const char *markup_end, bool is_unless);
static ast_node_t *parse_case(template_parser_t *parser, const char *markup, const char *markup_end);
static ast_node_t *parse_for(template_parser_t *parser, const char *markup, const char *markup_end);
static ast_node_t *parse_tablerow(template_parser_t *parser, const char *markup, const char *markup_end);
static ast_node_t *parse_assign(template_parser_t *parser, const char *markup, const char *markup_end);
static ast_node_t *parse_capture(template_parser_t *parser, const char *markup, const char *markup_end);
static ast_node_t *parse_increment(template_parser_t *parser, const char *markup, const char *markup_end);
static ast_node_t *parse_decrement(template_parser_t *parser, const char *markup, const char *markup_end);
static ast_node_t *parse_cycle(template_parser_t *parser, const char *markup, const char *markup_end);
static ast_node_t *parse_echo(template_parser_t *parser, const char *markup, const char *markup_end);
static ast_node_t *parse_include(template_parser_t *parser, const char *markup, const char *markup_end);
static ast_node_t *parse_render(template_parser_t *parser, const char *markup, const char *markup_end);
static ast_node_t *parse_comment(template_parser_t *parser);
static ast_node_t *parse_raw_tag(template_parser_t *parser);
static ast_node_t *parse_liquid_tag(template_parser_t *parser, const char *markup, const char *markup_end);

/* Helper: Check if string matches identifier */
static inline bool str_eq(const char *str, size_t len, const char *match)
{
    size_t match_len = strlen(match);
    return len == match_len && memcmp(str, match, len) == 0;
}

/* Helper: Check if character is identifier character */
static inline int is_id_char(int c)
{
    return rb_isalnum(c) || c == '_';
}

void template_parser_init(template_parser_t *parser,
                          VALUE tokenizer_obj,
                          VALUE parse_context)
{
    Tokenizer_Get_Struct(tokenizer_obj, parser->tokenizer);
    parser->tokenizer_obj = tokenizer_obj;
    parser->parse_context = parse_context;

    arena_init(&parser->arena);

    memset(&parser->current_token, 0, sizeof(token_t));
    parser->has_token = false;

    parser->error_exception = Qnil;
    parser->error_occurred = false;

    parser->root = NULL;

    parser->node_count = 0;
    parser->max_depth = 0;
    parser->current_depth = 0;

    parser->tag_registry = rb_funcall(cLiquidTemplate, intern_tags, 0);
}

void template_parser_free(template_parser_t *parser)
{
    arena_free(&parser->arena);
}

void template_parser_gc_mark(template_parser_t *parser)
{
    rb_gc_mark(parser->tokenizer_obj);
    rb_gc_mark(parser->parse_context);
    rb_gc_mark(parser->error_exception);
    rb_gc_mark(parser->tag_registry);

    if (parser->root != NULL) {
        ast_gc_mark(parser->root);
    }
}

static void template_parser_guard_mark(void *ptr)
{
    template_parser_t *parser = ptr;
    if (parser != NULL) {
        template_parser_gc_mark(parser);
    }
}

static const rb_data_type_t template_parser_guard_type = {
    "liquid_template_parser_guard",
    { template_parser_guard_mark, NULL, NULL, },
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
};

VALUE template_parser_gc_guard_new(template_parser_t *parser)
{
    return TypedData_Wrap_Struct(rb_cObject, &template_parser_guard_type, parser);
}

__attribute__((noreturn))
void template_parser_error(template_parser_t *parser, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char message[512];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    unsigned int line = parser->tokenizer->line_number;
    if (line > 0) {
        parser->error_exception = rb_exc_new_str(cLiquidSyntaxError,
            rb_sprintf("Liquid syntax error (line %u): %s", line, message));
    } else {
        parser->error_exception = rb_exc_new_str(cLiquidSyntaxError,
            rb_sprintf("Liquid syntax error: %s", message));
    }

    parser->error_occurred = true;
    longjmp(parser->error_jmp, 1);
}

__attribute__((noreturn))
void template_parser_tag_error(template_parser_t *parser,
                                const char *tag_name,
                                const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char message[512];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    unsigned int line = parser->tokenizer->line_number;
    if (line > 0) {
        parser->error_exception = rb_exc_new_str(cLiquidSyntaxError,
            rb_sprintf("Liquid syntax error (line %u): '%s' %s", line, tag_name, message));
    } else {
        parser->error_exception = rb_exc_new_str(cLiquidSyntaxError,
            rb_sprintf("Liquid syntax error: '%s' %s", tag_name, message));
    }

    parser->error_occurred = true;
    longjmp(parser->error_jmp, 1);
}

/* Get next token from tokenizer */
static void next_token(template_parser_t *parser)
{
    tokenizer_next(parser->tokenizer, &parser->current_token);
    parser->has_token = (parser->current_token.type != TOKENIZER_TOKEN_NONE);
}

/* Parse an expression with filters into bytecode */
void template_parser_parse_expression(template_parser_t *parser,
                                       const char *markup,
                                       const char *markup_end,
                                       vm_assembler_t *code)
{
    parser_t p;
    init_parser(&p, markup, markup_end);

    /* Parse the base expression */
    parse_and_compile_expression(&p, code);

    /* Parse filters (if any) */
    while (parser_consume(&p, TOKEN_PIPE).type) {
        lexer_token_t filter_name_token = parser_must_consume(&p, TOKEN_IDENTIFIER);
        VALUE filter_name = token_to_rsym(filter_name_token);

        size_t arg_count = 0;

        if (parser_consume(&p, TOKEN_COLON).type) {
            do {
                parse_and_compile_expression(&p, code);
                arg_count++;
            } while (parser_consume(&p, TOKEN_COMMA).type);
        }

        vm_assembler_add_filter(code, filter_name, arg_count);
    }
}

/* Parse comparison operator from token */
static comparison_op_t parse_comparison_op(const char *str, size_t len)
{
    if (len == 2) {
        if (memcmp(str, "==", 2) == 0) return CMP_EQ;
        if (memcmp(str, "!=", 2) == 0) return CMP_NE;
        if (memcmp(str, "<>", 2) == 0) return CMP_NE;
        if (memcmp(str, "<=", 2) == 0) return CMP_LE;
        if (memcmp(str, ">=", 2) == 0) return CMP_GE;
    } else if (len == 1) {
        if (*str == '<') return CMP_LT;
        if (*str == '>') return CMP_GT;
    } else if (len == 8 && memcmp(str, "contains", 8) == 0) {
        return CMP_CONTAINS;
    }
    return CMP_NONE;
}

/* Parse a condition with optional comparison and logical operators */
ast_condition_t *template_parser_parse_condition(template_parser_t *parser,
                                                  const char *markup,
                                                  const char *markup_end)
{
    ast_condition_t *first_cond = NULL;
    ast_condition_t *last_cond = NULL;

    const char *cur = markup;

    while (cur < markup_end) {
        /* Skip whitespace */
        while (cur < markup_end && rb_isspace(*cur)) cur++;
        if (cur >= markup_end) break;

        ast_condition_t *cond = ast_condition_alloc(&parser->arena);
        ast_init_assembler(&cond->left_expr);

        /* Find the extent of this condition (up to 'and' or 'or') */
        const char *cond_end = cur;
        int paren_depth = 0;
        bool in_string = false;
        char string_char = 0;

        while (cond_end < markup_end) {
            char c = *cond_end;

            if (in_string) {
                if (c == string_char) in_string = false;
            } else {
                if (c == '"' || c == '\'') {
                    in_string = true;
                    string_char = c;
                } else if (c == '(') {
                    paren_depth++;
                } else if (c == ')') {
                    paren_depth--;
                } else if (paren_depth == 0) {
                    /* Check for 'and' or 'or' */
                    size_t remaining = markup_end - cond_end;
                    if (remaining >= 4 && memcmp(cond_end, " and", 4) == 0 &&
                        (remaining == 4 || rb_isspace(cond_end[4]))) {
                        break;
                    }
                    if (remaining >= 3 && memcmp(cond_end, " or", 3) == 0 &&
                        (remaining == 3 || rb_isspace(cond_end[3]))) {
                        break;
                    }
                }
            }
            cond_end++;
        }

        /* Parse this condition segment */
        const char *seg_start = cur;
        const char *seg_end = cond_end;

        /* Skip trailing whitespace */
        while (seg_end > seg_start && rb_isspace(seg_end[-1])) seg_end--;

        /* Look for comparison operator */
        const char *comp_start = NULL;
        const char *comp_end = NULL;
        comparison_op_t comp_op = CMP_NONE;

        for (const char *p = seg_start; p < seg_end; p++) {
            char c = *p;
            if (c == '"' || c == '\'') {
                /* Skip string */
                char quote = c;
                p++;
                while (p < seg_end && *p != quote) p++;
            } else if (c == '=' && p + 1 < seg_end && p[1] == '=') {
                comp_start = p;
                comp_end = p + 2;
                comp_op = CMP_EQ;
                break;
            } else if (c == '!' && p + 1 < seg_end && p[1] == '=') {
                comp_start = p;
                comp_end = p + 2;
                comp_op = CMP_NE;
                break;
            } else if (c == '<') {
                if (p + 1 < seg_end && p[1] == '=') {
                    comp_start = p;
                    comp_end = p + 2;
                    comp_op = CMP_LE;
                } else if (p + 1 < seg_end && p[1] == '>') {
                    comp_start = p;
                    comp_end = p + 2;
                    comp_op = CMP_NE;
                } else {
                    comp_start = p;
                    comp_end = p + 1;
                    comp_op = CMP_LT;
                }
                break;
            } else if (c == '>') {
                if (p + 1 < seg_end && p[1] == '=') {
                    comp_start = p;
                    comp_end = p + 2;
                    comp_op = CMP_GE;
                } else {
                    comp_start = p;
                    comp_end = p + 1;
                    comp_op = CMP_GT;
                }
                break;
            } else if (seg_end - p >= 8 && memcmp(p, "contains", 8) == 0) {
                /* Make sure 'contains' is not part of identifier */
                if ((p == seg_start || !is_id_char(p[-1])) &&
                    (p + 8 >= seg_end || !is_id_char(p[8]))) {
                    comp_start = p;
                    comp_end = p + 8;
                    comp_op = CMP_CONTAINS;
                    break;
                }
            }
        }

        if (comp_op != CMP_NONE) {
            /* Parse left expression */
            const char *left_end = comp_start;
            while (left_end > seg_start && rb_isspace(left_end[-1])) left_end--;

            template_parser_parse_expression(parser, seg_start, left_end, &cond->left_expr);

            /* Parse right expression */
            const char *right_start = comp_end;
            while (right_start < seg_end && rb_isspace(*right_start)) right_start++;

            cond->comparison_op = comp_op;
            ast_init_assembler(&cond->right_expr);
            template_parser_parse_expression(parser, right_start, seg_end, &cond->right_expr);
        } else {
            /* Just a truthy check */
            template_parser_parse_expression(parser, seg_start, seg_end, &cond->left_expr);
            cond->comparison_op = CMP_NONE;
        }

        /* Link condition */
        if (last_cond != NULL) {
            last_cond->next = cond;
        } else {
            first_cond = cond;
        }
        last_cond = cond;

        /* Check for 'and' or 'or' */
        cur = cond_end;
        while (cur < markup_end && rb_isspace(*cur)) cur++;

        if (markup_end - cur >= 3 && memcmp(cur, "and", 3) == 0 &&
            (cur + 3 >= markup_end || rb_isspace(cur[3]))) {
            last_cond->logical_op = LOGIC_AND;
            cur += 3;
        } else if (markup_end - cur >= 2 && memcmp(cur, "or", 2) == 0 &&
                   (cur + 2 >= markup_end || rb_isspace(cur[2]))) {
            last_cond->logical_op = LOGIC_OR;
            cur += 2;
        } else {
            break;
        }
    }

    return first_cond;
}

/* Parse a raw text node */
static ast_node_t *parse_raw_text(template_parser_t *parser, token_t *token)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_RAW, parser->tokenizer->line_number);

    node->data.raw.text = arena_strdup(&parser->arena, token->str_full, token->len_full);
    node->data.raw.length = token->len_full;
    node->data.raw.lstrip = token->lstrip;
    node->data.raw.rstrip = token->rstrip;

    parser->node_count++;
    return node;
}

/* Parse a variable output {{ expression }} */
static ast_node_t *parse_variable(template_parser_t *parser, token_t *token)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_VARIABLE, parser->tokenizer->line_number);

    ast_init_assembler(&node->data.variable.expr);
    node->data.variable.line_number = parser->tokenizer->line_number;

    /* Use existing variable parsing from variable.c */
    parser_t p;
    init_parser(&p, token->str_trimmed, token->str_trimmed + token->len_trimmed);

    if (p.cur.type == TOKEN_EOS) {
        vm_assembler_add_push_nil(&node->data.variable.expr);
    } else {
        /* Parse expression with filters */
        parse_and_compile_expression(&p, &node->data.variable.expr);

        /* Parse filters */
        while (parser_consume(&p, TOKEN_PIPE).type) {
            lexer_token_t filter_name_token = parser_must_consume(&p, TOKEN_IDENTIFIER);
            VALUE filter_name = token_to_rsym(filter_name_token);

            size_t arg_count = 0;

            if (parser_consume(&p, TOKEN_COLON).type) {
                do {
                    parse_and_compile_expression(&p, &node->data.variable.expr);
                    arg_count++;
                } while (parser_consume(&p, TOKEN_COMMA).type);
            }

            vm_assembler_add_filter(&node->data.variable.expr, filter_name, arg_count);
        }
    }

    parser->node_count++;
    return node;
}

/* Parse if/unless tag */
static ast_node_t *parse_if(template_parser_t *parser, const char *markup, const char *markup_end, bool is_unless)
{
    ast_node_t *node = ast_node_alloc(&parser->arena,
        is_unless ? AST_UNLESS : AST_IF,
        parser->tokenizer->line_number);

    /* Parse initial condition */
    ast_branch_t *first_branch = ast_branch_alloc(&parser->arena);
    first_branch->condition = template_parser_parse_condition(parser, markup, markup_end);
    ast_node_list_init(&first_branch->body);

    node->data.conditional.branches = first_branch;
    ast_branch_t *last_branch = first_branch;

    parser->current_depth++;
    if (parser->current_depth > parser->max_depth) {
        parser->max_depth = parser->current_depth;
    }

    /* Parse body until elsif/else/endif */
    const char *end_tags[] = { "elsif", "else", is_unless ? "endunless" : "endif" };
    VALUE end_tag;

    while (true) {
        end_tag = template_parser_parse_body(parser, &last_branch->body, end_tags, 3);

        if (end_tag == Qnil) {
            template_parser_tag_error(parser, is_unless ? "unless" : "if",
                "tag was never closed");
        }

        const char *tag_name = RSTRING_PTR(end_tag);
        size_t tag_len = RSTRING_LEN(end_tag);

        if (str_eq(tag_name, tag_len, is_unless ? "endunless" : "endif")) {
            break;
        } else if (str_eq(tag_name, tag_len, "elsif")) {
            if (is_unless) {
                template_parser_tag_error(parser, "unless",
                    "'elsif' is not allowed in unless blocks");
            }

            /* Get elsif condition from next token markup */
            if (!parser->has_token) {
                template_parser_error(parser, "Unexpected end of template");
            }

            const char *elsif_markup = parser->current_token.str_trimmed;
            const char *elsif_end = elsif_markup + parser->current_token.len_trimmed;

            /* Skip "elsif" keyword */
            elsif_markup = read_while(elsif_markup, elsif_end, rb_isspace);
            elsif_markup += 5; /* "elsif" */
            elsif_markup = read_while(elsif_markup, elsif_end, rb_isspace);

            ast_branch_t *elsif_branch = ast_branch_alloc(&parser->arena);
            elsif_branch->condition = template_parser_parse_condition(parser, elsif_markup, elsif_end);
            ast_node_list_init(&elsif_branch->body);

            last_branch->next = elsif_branch;
            last_branch = elsif_branch;
        } else if (str_eq(tag_name, tag_len, "else")) {
            ast_branch_t *else_branch = ast_branch_alloc(&parser->arena);
            else_branch->condition = NULL; /* else has no condition */
            ast_node_list_init(&else_branch->body);

            last_branch->next = else_branch;
            last_branch = else_branch;

            /* Parse until endif */
            const char *final_tags[] = { is_unless ? "endunless" : "endif" };
            end_tag = template_parser_parse_body(parser, &last_branch->body, final_tags, 1);

            if (end_tag == Qnil) {
                template_parser_tag_error(parser, is_unless ? "unless" : "if",
                    "tag was never closed");
            }
            break;
        }
    }

    parser->current_depth--;
    parser->node_count++;
    return node;
}

/* Parse case tag */
static ast_node_t *parse_case(template_parser_t *parser, const char *markup, const char *markup_end)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_CASE, parser->tokenizer->line_number);

    /* Parse target expression */
    ast_init_assembler(&node->data.case_stmt.target_expr);
    template_parser_parse_expression(parser, markup, markup_end, &node->data.case_stmt.target_expr);

    node->data.case_stmt.branches = NULL;
    ast_branch_t *last_branch = NULL;

    parser->current_depth++;
    if (parser->current_depth > parser->max_depth) {
        parser->max_depth = parser->current_depth;
    }

    /* Parse when/else branches */
    const char *end_tags[] = { "when", "else", "endcase" };
    VALUE end_tag;

    while (true) {
        if (last_branch != NULL) {
            end_tag = template_parser_parse_body(parser, &last_branch->body, end_tags, 3);
        } else {
            /* Skip to first when/else/endcase */
            ast_node_list_t dummy;
            ast_node_list_init(&dummy);
            end_tag = template_parser_parse_body(parser, &dummy, end_tags, 3);
        }

        if (end_tag == Qnil) {
            template_parser_tag_error(parser, "case", "tag was never closed");
        }

        const char *tag_name = RSTRING_PTR(end_tag);
        size_t tag_len = RSTRING_LEN(end_tag);

        if (str_eq(tag_name, tag_len, "endcase")) {
            break;
        } else if (str_eq(tag_name, tag_len, "when")) {
            /* Get when values from current token */
            if (!parser->has_token) {
                template_parser_error(parser, "Unexpected end of template");
            }

            const char *when_markup = parser->current_token.str_trimmed;
            const char *when_end = when_markup + parser->current_token.len_trimmed;

            /* Skip "when" keyword */
            when_markup = read_while(when_markup, when_end, rb_isspace);
            when_markup += 4; /* "when" */
            when_markup = read_while(when_markup, when_end, rb_isspace);

            ast_branch_t *when_branch = ast_branch_alloc(&parser->arena);

            /* Parse when values as conditions */
            /* For case/when, we store the values as a special condition */
            when_branch->condition = ast_condition_alloc(&parser->arena);
            ast_init_assembler(&when_branch->condition->left_expr);
            template_parser_parse_expression(parser, when_markup, when_end, &when_branch->condition->left_expr);

            ast_node_list_init(&when_branch->body);

            if (last_branch != NULL) {
                last_branch->next = when_branch;
            } else {
                node->data.case_stmt.branches = when_branch;
            }
            last_branch = when_branch;
        } else if (str_eq(tag_name, tag_len, "else")) {
            ast_branch_t *else_branch = ast_branch_alloc(&parser->arena);
            else_branch->condition = NULL;
            ast_node_list_init(&else_branch->body);

            if (last_branch != NULL) {
                last_branch->next = else_branch;
            } else {
                node->data.case_stmt.branches = else_branch;
            }
            last_branch = else_branch;

            /* Parse until endcase */
            const char *final_tags[] = { "endcase" };
            end_tag = template_parser_parse_body(parser, &last_branch->body, final_tags, 1);

            if (end_tag == Qnil) {
                template_parser_tag_error(parser, "case", "tag was never closed");
            }
            break;
        }
    }

    parser->current_depth--;
    parser->node_count++;
    return node;
}

/* Parse for loop parameters */
static void parse_for_params(template_parser_t *parser,
                              const char *markup, const char *markup_end,
                              ast_for_params_t *params)
{
    params->has_limit = false;
    params->has_offset = false;
    params->reversed = false;

    const char *cur = markup;

    while (cur < markup_end) {
        while (cur < markup_end && rb_isspace(*cur)) cur++;
        if (cur >= markup_end) break;

        /* Check for 'reversed' */
        if (markup_end - cur >= 8 && memcmp(cur, "reversed", 8) == 0 &&
            (cur + 8 >= markup_end || !is_id_char(cur[8]))) {
            params->reversed = true;
            cur += 8;
            continue;
        }

        /* Check for 'limit:' */
        if (markup_end - cur >= 6 && memcmp(cur, "limit:", 6) == 0) {
            cur += 6;
            while (cur < markup_end && rb_isspace(*cur)) cur++;

            /* Find end of expression */
            const char *expr_end = cur;
            while (expr_end < markup_end && !rb_isspace(*expr_end)) expr_end++;

            ast_init_assembler(&params->limit_expr);
            template_parser_parse_expression(parser, cur, expr_end, &params->limit_expr);
            params->has_limit = true;
            cur = expr_end;
            continue;
        }

        /* Check for 'offset:' */
        if (markup_end - cur >= 7 && memcmp(cur, "offset:", 7) == 0) {
            cur += 7;
            while (cur < markup_end && rb_isspace(*cur)) cur++;

            /* Find end of expression */
            const char *expr_end = cur;
            while (expr_end < markup_end && !rb_isspace(*expr_end)) expr_end++;

            ast_init_assembler(&params->offset_expr);
            template_parser_parse_expression(parser, cur, expr_end, &params->offset_expr);
            params->has_offset = true;
            cur = expr_end;
            continue;
        }

        /* Unknown parameter, skip */
        while (cur < markup_end && !rb_isspace(*cur)) cur++;
    }
}

/* Parse for tag */
static ast_node_t *parse_for(template_parser_t *parser, const char *markup, const char *markup_end)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_FOR, parser->tokenizer->line_number);

    /* Parse: variable_name in collection [limit:n] [offset:n] [reversed] */
    const char *cur = markup;
    while (cur < markup_end && rb_isspace(*cur)) cur++;

    /* Get variable name */
    const char *var_start = cur;
    while (cur < markup_end && is_id_char(*cur)) cur++;
    const char *var_end = cur;

    if (var_start == var_end) {
        template_parser_tag_error(parser, "for", "expected variable name");
    }

    node->data.for_loop.var_name = rb_enc_str_new(var_start, var_end - var_start, utf8_encoding);

    /* Expect 'in' */
    while (cur < markup_end && rb_isspace(*cur)) cur++;
    if (markup_end - cur < 2 || memcmp(cur, "in", 2) != 0) {
        template_parser_tag_error(parser, "for", "expected 'in'");
    }
    cur += 2;
    while (cur < markup_end && rb_isspace(*cur)) cur++;

    /* Parse collection expression - find where parameters start */
    const char *collection_end = cur;
    while (collection_end < markup_end) {
        /* Check for parameter keywords */
        if (markup_end - collection_end >= 6 && memcmp(collection_end, "limit:", 6) == 0) break;
        if (markup_end - collection_end >= 7 && memcmp(collection_end, "offset:", 7) == 0) break;
        if (markup_end - collection_end >= 8 && memcmp(collection_end, "reversed", 8) == 0 &&
            (collection_end + 8 >= markup_end || !is_id_char(collection_end[8]))) break;
        collection_end++;
    }

    /* Trim trailing whitespace from collection */
    while (collection_end > cur && rb_isspace(collection_end[-1])) collection_end--;

    ast_init_assembler(&node->data.for_loop.collection);
    template_parser_parse_expression(parser, cur, collection_end, &node->data.for_loop.collection);

    /* Parse parameters */
    parse_for_params(parser, collection_end, markup_end, &node->data.for_loop.params);

    ast_node_list_init(&node->data.for_loop.body);
    ast_node_list_init(&node->data.for_loop.else_body);
    node->data.for_loop.has_else = false;

    parser->current_depth++;
    if (parser->current_depth > parser->max_depth) {
        parser->max_depth = parser->current_depth;
    }

    /* Parse body */
    const char *end_tags[] = { "else", "endfor" };
    VALUE end_tag = template_parser_parse_body(parser, &node->data.for_loop.body, end_tags, 2);

    if (end_tag == Qnil) {
        template_parser_tag_error(parser, "for", "tag was never closed");
    }

    const char *tag_name = RSTRING_PTR(end_tag);
    size_t tag_len = RSTRING_LEN(end_tag);

    if (str_eq(tag_name, tag_len, "else")) {
        node->data.for_loop.has_else = true;

        const char *final_tags[] = { "endfor" };
        end_tag = template_parser_parse_body(parser, &node->data.for_loop.else_body, final_tags, 1);

        if (end_tag == Qnil) {
            template_parser_tag_error(parser, "for", "tag was never closed");
        }
    }

    parser->current_depth--;
    parser->node_count++;
    return node;
}

/* Parse tablerow tag */
static ast_node_t *parse_tablerow(template_parser_t *parser, const char *markup, const char *markup_end)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_TABLEROW, parser->tokenizer->line_number);

    /* Similar to for loop parsing */
    const char *cur = markup;
    while (cur < markup_end && rb_isspace(*cur)) cur++;

    /* Get variable name */
    const char *var_start = cur;
    while (cur < markup_end && is_id_char(*cur)) cur++;
    const char *var_end = cur;

    if (var_start == var_end) {
        template_parser_tag_error(parser, "tablerow", "expected variable name");
    }

    node->data.tablerow.var_name = rb_enc_str_new(var_start, var_end - var_start, utf8_encoding);

    /* Expect 'in' */
    while (cur < markup_end && rb_isspace(*cur)) cur++;
    if (markup_end - cur < 2 || memcmp(cur, "in", 2) != 0) {
        template_parser_tag_error(parser, "tablerow", "expected 'in'");
    }
    cur += 2;
    while (cur < markup_end && rb_isspace(*cur)) cur++;

    /* Parse collection expression */
    const char *collection_end = cur;
    while (collection_end < markup_end) {
        if (markup_end - collection_end >= 5 && memcmp(collection_end, "cols:", 5) == 0) break;
        if (markup_end - collection_end >= 6 && memcmp(collection_end, "limit:", 6) == 0) break;
        if (markup_end - collection_end >= 7 && memcmp(collection_end, "offset:", 7) == 0) break;
        collection_end++;
    }
    while (collection_end > cur && rb_isspace(collection_end[-1])) collection_end--;

    ast_init_assembler(&node->data.tablerow.collection);
    template_parser_parse_expression(parser, cur, collection_end, &node->data.tablerow.collection);

    /* Parse parameters including cols */
    parse_for_params(parser, collection_end, markup_end, &node->data.tablerow.params);

    /* Check for cols: parameter */
    node->data.tablerow.has_cols = false;
    cur = collection_end;
    while (cur < markup_end) {
        while (cur < markup_end && rb_isspace(*cur)) cur++;
        if (markup_end - cur >= 5 && memcmp(cur, "cols:", 5) == 0) {
            cur += 5;
            while (cur < markup_end && rb_isspace(*cur)) cur++;

            const char *expr_end = cur;
            while (expr_end < markup_end && !rb_isspace(*expr_end)) expr_end++;

            ast_init_assembler(&node->data.tablerow.cols_expr);
            template_parser_parse_expression(parser, cur, expr_end, &node->data.tablerow.cols_expr);
            node->data.tablerow.has_cols = true;
            break;
        }
        while (cur < markup_end && !rb_isspace(*cur)) cur++;
    }

    ast_node_list_init(&node->data.tablerow.body);

    parser->current_depth++;
    if (parser->current_depth > parser->max_depth) {
        parser->max_depth = parser->current_depth;
    }

    /* Parse body */
    const char *end_tags[] = { "endtablerow" };
    VALUE end_tag = template_parser_parse_body(parser, &node->data.tablerow.body, end_tags, 1);

    if (end_tag == Qnil) {
        template_parser_tag_error(parser, "tablerow", "tag was never closed");
    }

    parser->current_depth--;
    parser->node_count++;
    return node;
}

/* Parse assign tag */
static ast_node_t *parse_assign(template_parser_t *parser, const char *markup, const char *markup_end)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_ASSIGN, parser->tokenizer->line_number);

    const char *cur = markup;
    while (cur < markup_end && rb_isspace(*cur)) cur++;

    /* Get variable name */
    const char *var_start = cur;
    while (cur < markup_end && is_id_char(*cur)) cur++;
    const char *var_end = cur;

    if (var_start == var_end) {
        template_parser_tag_error(parser, "assign", "expected variable name");
    }

    node->data.assign.var_name = rb_enc_str_new(var_start, var_end - var_start, utf8_encoding);

    /* Expect '=' */
    while (cur < markup_end && rb_isspace(*cur)) cur++;
    if (cur >= markup_end || *cur != '=') {
        template_parser_tag_error(parser, "assign", "expected '='");
    }
    cur++;
    while (cur < markup_end && rb_isspace(*cur)) cur++;

    /* Parse expression */
    ast_init_assembler(&node->data.assign.expr);
    template_parser_parse_expression(parser, cur, markup_end, &node->data.assign.expr);

    parser->node_count++;
    return node;
}

/* Parse capture tag */
static ast_node_t *parse_capture(template_parser_t *parser, const char *markup, const char *markup_end)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_CAPTURE, parser->tokenizer->line_number);

    const char *cur = markup;
    while (cur < markup_end && rb_isspace(*cur)) cur++;

    /* Get variable name */
    const char *var_start = cur;
    while (cur < markup_end && is_id_char(*cur)) cur++;
    const char *var_end = cur;

    if (var_start == var_end) {
        template_parser_tag_error(parser, "capture", "expected variable name");
    }

    node->data.capture.var_name = rb_enc_str_new(var_start, var_end - var_start, utf8_encoding);
    ast_node_list_init(&node->data.capture.body);

    parser->current_depth++;
    if (parser->current_depth > parser->max_depth) {
        parser->max_depth = parser->current_depth;
    }

    /* Parse body */
    const char *end_tags[] = { "endcapture" };
    VALUE end_tag = template_parser_parse_body(parser, &node->data.capture.body, end_tags, 1);

    if (end_tag == Qnil) {
        template_parser_tag_error(parser, "capture", "tag was never closed");
    }

    parser->current_depth--;
    parser->node_count++;
    return node;
}

/* Parse increment tag */
static ast_node_t *parse_increment(template_parser_t *parser, const char *markup, const char *markup_end)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_INCREMENT, parser->tokenizer->line_number);

    const char *cur = markup;
    while (cur < markup_end && rb_isspace(*cur)) cur++;

    const char *var_start = cur;
    while (cur < markup_end && is_id_char(*cur)) cur++;

    if (var_start == cur) {
        template_parser_tag_error(parser, "increment", "expected variable name");
    }

    node->data.counter.var_name = rb_enc_str_new(var_start, cur - var_start, utf8_encoding);

    parser->node_count++;
    return node;
}

/* Parse decrement tag */
static ast_node_t *parse_decrement(template_parser_t *parser, const char *markup, const char *markup_end)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_DECREMENT, parser->tokenizer->line_number);

    const char *cur = markup;
    while (cur < markup_end && rb_isspace(*cur)) cur++;

    const char *var_start = cur;
    while (cur < markup_end && is_id_char(*cur)) cur++;

    if (var_start == cur) {
        template_parser_tag_error(parser, "decrement", "expected variable name");
    }

    node->data.counter.var_name = rb_enc_str_new(var_start, cur - var_start, utf8_encoding);

    parser->node_count++;
    return node;
}

/* Parse cycle tag */
static ast_node_t *parse_cycle(template_parser_t *parser, const char *markup, const char *markup_end)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_CYCLE, parser->tokenizer->line_number);

    node->data.cycle.group_name = Qnil;
    node->data.cycle.values = NULL;
    node->data.cycle.value_count = 0;

    /* Check for group name: "group_name: val1, val2" or "val1, val2" */
    const char *cur = markup;
    while (cur < markup_end && rb_isspace(*cur)) cur++;

    /* Look for colon to detect group name */
    const char *colon = memchr(cur, ':', markup_end - cur);
    const char *values_start = cur;

    if (colon != NULL) {
        /* Check if this is a group name (quoted string or identifier before colon) */
        const char *p = cur;
        bool has_group = false;

        if (*p == '"' || *p == '\'') {
            /* Quoted group name */
            char quote = *p++;
            const char *group_start = p;
            while (p < colon && *p != quote) p++;
            if (p < colon) {
                node->data.cycle.group_name = rb_enc_str_new(group_start, p - group_start, utf8_encoding);
                has_group = true;
                values_start = colon + 1;
            }
        } else if (is_id_char(*p)) {
            /* Identifier group name */
            const char *group_start = p;
            while (p < colon && is_id_char(*p)) p++;
            while (p < colon && rb_isspace(*p)) p++;
            if (p == colon) {
                node->data.cycle.group_name = rb_enc_str_new(group_start, p - group_start - (p - group_start > 0 && rb_isspace(p[-1]) ? 1 : 0), utf8_encoding);
                has_group = true;
                values_start = colon + 1;
            }
        }

        if (!has_group) {
            values_start = cur;
        }
    }

    /* Parse comma-separated values */
    size_t capacity = 4;
    node->data.cycle.values = arena_alloc(&parser->arena, capacity * sizeof(vm_assembler_t));

    cur = values_start;
    while (cur < markup_end) {
        while (cur < markup_end && rb_isspace(*cur)) cur++;
        if (cur >= markup_end) break;

        /* Find end of value (comma or end) */
        const char *val_end = cur;
        bool in_string = false;
        char string_char = 0;

        while (val_end < markup_end) {
            char c = *val_end;
            if (in_string) {
                if (c == string_char) in_string = false;
            } else {
                if (c == '"' || c == '\'') {
                    in_string = true;
                    string_char = c;
                } else if (c == ',') {
                    break;
                }
            }
            val_end++;
        }

        /* Trim trailing whitespace */
        const char *val_trimmed = val_end;
        while (val_trimmed > cur && rb_isspace(val_trimmed[-1])) val_trimmed--;

        if (val_trimmed > cur) {
            if (node->data.cycle.value_count >= capacity) {
                capacity *= 2;
                vm_assembler_t *new_values = arena_alloc(&parser->arena, capacity * sizeof(vm_assembler_t));
                memcpy(new_values, node->data.cycle.values, node->data.cycle.value_count * sizeof(vm_assembler_t));
                node->data.cycle.values = new_values;
            }

            vm_assembler_t *value = &node->data.cycle.values[node->data.cycle.value_count++];
            ast_init_assembler(value);
            template_parser_parse_expression(parser, cur, val_trimmed, value);
        }

        cur = val_end;
        if (cur < markup_end && *cur == ',') cur++;
    }

    parser->node_count++;
    return node;
}

/* Parse echo tag */
static ast_node_t *parse_echo(template_parser_t *parser, const char *markup, const char *markup_end)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_ECHO, parser->tokenizer->line_number);

    ast_init_assembler(&node->data.echo.expr);
    node->data.echo.line_number = parser->tokenizer->line_number;

    template_parser_parse_expression(parser, markup, markup_end, &node->data.echo.expr);

    parser->node_count++;
    return node;
}

/* Parse include tag */
static ast_node_t *parse_include(template_parser_t *parser, const char *markup, const char *markup_end)
{
    /* For now, delegate to Ruby as include/render are complex */
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_CUSTOM_TAG, parser->tokenizer->line_number);

    node->data.custom_tag.tag_name = rb_str_new_literal("include");
    node->data.custom_tag.markup = rb_enc_str_new(markup, markup_end - markup, utf8_encoding);

    VALUE tag_class = rb_funcall(parser->tag_registry, intern_square_brackets, 1, node->data.custom_tag.tag_name);
    if (tag_class != Qnil) {
        node->data.custom_tag.tag_obj = rb_funcall(tag_class, intern_parse, 4,
            node->data.custom_tag.tag_name, node->data.custom_tag.markup,
            parser->tokenizer_obj, parser->parse_context);
    } else {
        node->data.custom_tag.tag_obj = Qnil;
    }

    parser->node_count++;
    return node;
}

/* Parse render tag */
static ast_node_t *parse_render(template_parser_t *parser, const char *markup, const char *markup_end)
{
    /* For now, delegate to Ruby as include/render are complex */
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_CUSTOM_TAG, parser->tokenizer->line_number);

    node->data.custom_tag.tag_name = rb_str_new_literal("render");
    node->data.custom_tag.markup = rb_enc_str_new(markup, markup_end - markup, utf8_encoding);

    VALUE tag_class = rb_funcall(parser->tag_registry, intern_square_brackets, 1, node->data.custom_tag.tag_name);
    if (tag_class != Qnil) {
        node->data.custom_tag.tag_obj = rb_funcall(tag_class, intern_parse, 4,
            node->data.custom_tag.tag_name, node->data.custom_tag.markup,
            parser->tokenizer_obj, parser->parse_context);
    } else {
        node->data.custom_tag.tag_obj = Qnil;
    }

    parser->node_count++;
    return node;
}

/* Parse comment tag - skip until endcomment */
static ast_node_t *parse_comment(template_parser_t *parser)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_COMMENT, parser->tokenizer->line_number);

    /* Skip tokens until endcomment */
    while (true) {
        next_token(parser);
        if (!parser->has_token) {
            template_parser_tag_error(parser, "comment", "tag was never closed");
        }

        if (parser->current_token.type == TOKEN_TAG) {
            const char *tag_start = parser->current_token.str_trimmed;
            const char *tag_end = tag_start + parser->current_token.len_trimmed;

            const char *name_start = read_while(tag_start, tag_end, rb_isspace);
            const char *name_end = read_while(name_start, tag_end, is_id_char);
            size_t name_len = name_end - name_start;

            if (str_eq(name_start, name_len, "endcomment")) {
                break;
            }
        }
    }

    parser->node_count++;
    return node;
}

/* Parse raw tag - capture literal content until endraw */
static ast_node_t *parse_raw_tag(template_parser_t *parser)
{
    /* For now, delegate to the existing raw tag handling */
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_CUSTOM_TAG, parser->tokenizer->line_number);

    node->data.custom_tag.tag_name = rb_str_new_literal("raw");
    node->data.custom_tag.markup = rb_str_new_literal("");

    VALUE tag_class = rb_funcall(parser->tag_registry, intern_square_brackets, 1, node->data.custom_tag.tag_name);
    if (tag_class != Qnil) {
        node->data.custom_tag.tag_obj = rb_funcall(tag_class, intern_parse, 4,
            node->data.custom_tag.tag_name, node->data.custom_tag.markup,
            parser->tokenizer_obj, parser->parse_context);
    } else {
        node->data.custom_tag.tag_obj = Qnil;
    }

    parser->node_count++;
    return node;
}

/* Parse liquid tag (multiline tag syntax) */
static ast_node_t *parse_liquid_tag(template_parser_t *parser, const char *markup, const char *markup_end)
{
    ast_node_t *node = ast_node_alloc(&parser->arena, AST_LIQUID_TAG, parser->tokenizer->line_number);
    ast_node_list_init(&node->data.liquid_tag.statements);

    /* Save tokenizer state */
    tokenizer_t saved_tokenizer = *parser->tokenizer;

    /* Setup tokenizer for liquid tag content */
    int line_number = parser->tokenizer->line_number;
    tokenizer_setup_for_liquid_tag(parser->tokenizer, markup, markup_end, line_number);

    /* Parse each line as a tag */
    while (true) {
        next_token(parser);
        if (!parser->has_token || parser->current_token.type == TOKENIZER_TOKEN_NONE) {
            break;
        }

        if (parser->current_token.type == TOKEN_BLANK_LIQUID_TAG_LINE) {
            continue;
        }

        if (parser->current_token.type == TOKEN_TAG) {
            ast_node_t *stmt = parse_tag(parser, &parser->current_token);
            if (stmt != NULL) {
                ast_node_list_append(&node->data.liquid_tag.statements, stmt, &parser->arena);
            }
        }
    }

    /* Restore tokenizer */
    *parser->tokenizer = saved_tokenizer;

    parser->node_count++;
    return node;
}

/* Parse a tag and return the appropriate AST node */
static ast_node_t *parse_tag(template_parser_t *parser, token_t *token)
{
    const char *tag_start = token->str_trimmed;
    const char *tag_end = tag_start + token->len_trimmed;

    /* Extract tag name */
    const char *name_start = read_while(tag_start, tag_end, rb_isspace);
    const char *name_end = read_while(name_start, tag_end, is_id_char);
    size_t name_len = name_end - name_start;

    if (name_len == 0) {
        /* Inline comment (#) */
        if (name_start < tag_end && *name_start == '#') {
            return ast_node_alloc(&parser->arena, AST_COMMENT, parser->tokenizer->line_number);
        }
        return NULL;
    }

    /* Get markup (content after tag name) */
    const char *markup = read_while(name_end, tag_end, rb_isspace);
    const char *markup_end = tag_end;

    /* Dispatch to appropriate parser */
    if (str_eq(name_start, name_len, "if")) {
        return parse_if(parser, markup, markup_end, false);
    } else if (str_eq(name_start, name_len, "unless")) {
        return parse_if(parser, markup, markup_end, true);
    } else if (str_eq(name_start, name_len, "case")) {
        return parse_case(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "for")) {
        return parse_for(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "tablerow")) {
        return parse_tablerow(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "assign")) {
        return parse_assign(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "capture")) {
        return parse_capture(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "increment")) {
        return parse_increment(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "decrement")) {
        return parse_decrement(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "cycle")) {
        return parse_cycle(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "echo")) {
        return parse_echo(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "include")) {
        return parse_include(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "render")) {
        return parse_render(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "comment")) {
        return parse_comment(parser);
    } else if (str_eq(name_start, name_len, "raw")) {
        return parse_raw_tag(parser);
    } else if (str_eq(name_start, name_len, "liquid")) {
        return parse_liquid_tag(parser, markup, markup_end);
    } else if (str_eq(name_start, name_len, "break")) {
        return ast_node_alloc(&parser->arena, AST_BREAK, parser->tokenizer->line_number);
    } else if (str_eq(name_start, name_len, "continue")) {
        return ast_node_alloc(&parser->arena, AST_CONTINUE, parser->tokenizer->line_number);
    } else {
        /* Unknown tag - delegate to Ruby */
        VALUE tag_name_str = rb_enc_str_new(name_start, name_len, utf8_encoding);
        VALUE tag_class = rb_funcall(parser->tag_registry, intern_square_brackets, 1, tag_name_str);

        if (tag_class == Qnil) {
            /* Truly unknown tag - return info for caller */
            return NULL;
        }

        /* Custom tag - parse via Ruby */
        ast_node_t *node = ast_node_alloc(&parser->arena, AST_CUSTOM_TAG, parser->tokenizer->line_number);
        node->data.custom_tag.tag_name = tag_name_str;
        node->data.custom_tag.markup = rb_enc_str_new(markup, markup_end - markup, utf8_encoding);
        node->data.custom_tag.tag_obj = rb_funcall(tag_class, intern_parse, 4,
            tag_name_str, node->data.custom_tag.markup,
            parser->tokenizer_obj, parser->parse_context);

        parser->node_count++;
        return node;
    }
}

/* Parse body until one of the end tags is encountered */
VALUE template_parser_parse_body(template_parser_t *parser,
                                  ast_node_list_t *body,
                                  const char **end_tags,
                                  size_t end_tag_count)
{
    while (true) {
        next_token(parser);
        if (!parser->has_token) {
            return Qnil;
        }

        token_t *token = &parser->current_token;

        switch (token->type) {
            case TOKEN_RAW:
            {
                ast_node_t *node = parse_raw_text(parser, token);
                ast_node_list_append(body, node, &parser->arena);
                break;
            }

            case TOKEN_VARIABLE:
            {
                ast_node_t *node = parse_variable(parser, token);
                ast_node_list_append(body, node, &parser->arena);
                break;
            }

            case TOKEN_TAG:
            {
                /* Check if this is an end tag */
                const char *tag_start = token->str_trimmed;
                const char *tag_end = tag_start + token->len_trimmed;

                const char *name_start = read_while(tag_start, tag_end, rb_isspace);
                const char *name_end = read_while(name_start, tag_end, is_id_char);
                size_t name_len = name_end - name_start;

                for (size_t i = 0; i < end_tag_count; i++) {
                    if (str_eq(name_start, name_len, end_tags[i])) {
                        return rb_enc_str_new(name_start, name_len, utf8_encoding);
                    }
                }

                /* Not an end tag, parse it */
                ast_node_t *node = parse_tag(parser, token);
                if (node != NULL) {
                    ast_node_list_append(body, node, &parser->arena);
                } else {
                    /* Unknown tag - return it */
                    return rb_enc_str_new(name_start, name_len, utf8_encoding);
                }
                break;
            }

            case TOKEN_INVALID:
                template_parser_error(parser, "Unexpected character in template");
                break;

            case TOKEN_BLANK_LIQUID_TAG_LINE:
                /* Skip blank lines in liquid tags */
                break;

            default:
                break;
        }
    }
}

/* Main parse function */
ast_node_t *template_parser_parse(template_parser_t *parser)
{
    if (setjmp(parser->error_jmp)) {
        /* Error occurred */
        return NULL;
    }

    parser->root = ast_node_alloc(&parser->arena, AST_TEMPLATE, 0);
    ast_node_list_init(&parser->root->data.template.children);

    /* Parse until EOF */
    const char *no_end_tags[] = {};
    VALUE end_tag = template_parser_parse_body(parser,
        &parser->root->data.template.children,
        no_end_tags, 0);

    if (end_tag != Qnil) {
        template_parser_error(parser, "Unexpected tag '%s'", RSTRING_PTR(end_tag));
    }

    return parser->root;
}

/* Module initialization */
void liquid_define_template_parser(void)
{
    intern_parse = rb_intern("parse");
    intern_square_brackets = rb_intern("[]");
    intern_tags = rb_intern("tags");
}
