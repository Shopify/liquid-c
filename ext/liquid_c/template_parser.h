#ifndef LIQUID_TEMPLATE_PARSER_H
#define LIQUID_TEMPLATE_PARSER_H

#include <ruby.h>
#include <setjmp.h>
#include "arena.h"
#include "ast.h"
#include "tokenizer.h"
#include "parser.h"

/*
 * Template parser for Liquid control flow tags.
 * Parses templates into an AST which is then compiled to bytecode.
 */

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
    bool has_token;                 /* True if current_token is valid */

    /* Error handling */
    jmp_buf error_jmp;
    VALUE error_exception;
    bool error_occurred;

    /* Output */
    ast_node_t *root;

    /* Statistics */
    unsigned int node_count;
    unsigned int max_depth;
    unsigned int current_depth;

    /* Tag registry for custom tags */
    VALUE tag_registry;
} template_parser_t;

/* Initialize parser */
void template_parser_init(template_parser_t *parser,
                          VALUE tokenizer_obj,
                          VALUE parse_context);

/* Parse template, returns root AST node */
ast_node_t *template_parser_parse(template_parser_t *parser);

/* Free parser resources */
void template_parser_free(template_parser_t *parser);

/* Mark parser for GC */
void template_parser_gc_mark(template_parser_t *parser);

/* Create a GC guard object for stack-allocated parser */
VALUE template_parser_gc_guard_new(template_parser_t *parser);

/* Parse a block body until an end tag or specific tag is encountered.
 * Returns the name of the terminating tag (or Qnil if EOF).
 * Appends nodes to the provided list. */
VALUE template_parser_parse_body(template_parser_t *parser,
                                  ast_node_list_t *body,
                                  const char **end_tags,
                                  size_t end_tag_count);

/* Parse an expression and compile it to bytecode */
void template_parser_parse_expression(template_parser_t *parser,
                                       const char *markup,
                                       const char *markup_end,
                                       vm_assembler_t *code);

/* Parse a condition (with and/or/comparisons) */
ast_condition_t *template_parser_parse_condition(template_parser_t *parser,
                                                  const char *markup,
                                                  const char *markup_end);

/* Raise a syntax error */
__attribute__((noreturn))
void template_parser_error(template_parser_t *parser, const char *format, ...);

/* Raise a syntax error with tag context */
__attribute__((noreturn))
void template_parser_tag_error(template_parser_t *parser,
                                const char *tag_name,
                                const char *format, ...);

/* Module initialization */
void liquid_define_template_parser(void);

#endif /* LIQUID_TEMPLATE_PARSER_H */
