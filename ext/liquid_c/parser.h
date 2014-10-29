#if !defined(LIQUID_PARSER_H)
#define LIQUID_PARSER_H

#include "lexer.h"

typedef struct parser {
    lexer_token_t cur, next;
    const char *str, *str_end;
} parser_t;

void init_parser(parser_t *parser, const char *str, const char *end);

lexer_token_t parser_must_consume(parser_t *parser, unsigned char type);
lexer_token_t parser_consume(parser_t *parser, unsigned char type);
lexer_token_t parser_consume_any(parser_t *parser);

VALUE parse_expression(parser_t *parser);

void init_liquid_parser(void);

#endif

