#include "liquid.h"
#include "parser.h"
#include "lexer.h"

void init_parser(parser_t *parser, const char *str, const char *end)
{
    parser->str = str;
    parser->str_end = end;
    parser->cur.type = TOKEN_EOS;
    parser->next.type = TOKEN_EOS;
    parser->str = lex_one(parser->str, parser->str_end, &parser->cur);
    parser->str = lex_one(parser->str, parser->str_end, &parser->next);
}

lexer_token_t parser_consume_any(parser_t *parser)
{
    lexer_token_t cur = parser->cur;
    parser->cur = parser->next;
    parser->next.type = TOKEN_EOS;
    parser->str = lex_one(parser->str, parser->str_end, &parser->next);
    return cur;
}

lexer_token_t parser_must_consume(parser_t *parser, unsigned char type)
{
    if (parser->cur.type != type) {
        rb_raise(
            cLiquidSyntaxError, "Expected %s but found %s",
            symbol_names[type], symbol_names[parser->cur.type]
        );
    }
    return parser_consume_any(parser);
}

lexer_token_t parser_consume(parser_t *parser, unsigned char type)
{
    if (parser->cur.type != type) {
        lexer_token_t zero;
        zero.type = 0;
        return zero;
    }
    return parser_consume_any(parser);
}

VALUE parse_expression(parser_t *parser)
{
    lexer_token_t token;

    switch (parser->cur.type) {
        case TOKEN_IDENTIFIER:
            return parse_variable_signature(parser);

        case TOKEN_QUOTE:
        case TOKEN_NUMBER:
            token = parser_consume_any(parser);
            return TOKEN_STR(token);

        case TOKEN_OPEN_ROUND:
        {
            parser_consume_any(parser);
            VALUE first = parse_expression(parser);
            parser_must_consume(parser, TOKEN_DOTDOT);
            VALUE last = parse_expression(parser);
            parser_must_consume(parser, TOKEN_CLOSE_ROUND);

            VALUE out = rb_enc_str_new("(", 1, utf8_encoding);
            rb_str_append(out, first);
            rb_str_append(out, rb_str_new2(".."));
            rb_str_append(out, last);
            rb_str_append(out, rb_str_new2(")"));
            return out;
        }
    }
    if (parser->cur.type == TOKEN_EOS) {
        rb_raise(cLiquidSyntaxError, "[:%s] is not a valid expression", symbol_names[parser->cur.type]);
    } else {
        rb_raise(
            cLiquidSyntaxError, "[:%s, \"%.*s\"] is not a valid expression",
            symbol_names[parser->cur.type], (int)(parser->cur.val_end - parser->cur.val), parser->cur.val
        );
    }
    return Qnil;
}

VALUE parse_argument(parser_t *parser)
{
    VALUE str = rb_enc_str_new("", 0, utf8_encoding);

    if (parser->cur.type == TOKEN_IDENTIFIER && parser->next.type == TOKEN_COLON) {
        lexer_token_t token = parser_consume_any(parser);
        rb_str_append(str, TOKEN_STR(token));

        parser_consume_any(parser);
        rb_str_append(str, rb_str_new2(": "));
    }

    return rb_str_append(str, parse_expression(parser));
}

VALUE parse_variable_signature(parser_t *parser)
{
    lexer_token_t token = parser_must_consume(parser, TOKEN_IDENTIFIER);
    VALUE str = TOKEN_STR(token);

    if (parser->cur.type == TOKEN_OPEN_SQUARE) {
        token = parser_consume_any(parser);
        rb_str_append(str, TOKEN_STR(token));

        rb_str_append(str, parse_expression(parser));

        token = parser_must_consume(parser, TOKEN_CLOSE_SQUARE);
        rb_str_append(str, TOKEN_STR(token));
    }

    if (parser->cur.type == TOKEN_DOT) {
        token = parser_consume_any(parser);
        rb_str_append(str, TOKEN_STR(token));

        rb_str_append(str, parse_variable_signature(parser));
    }

    return str;
}

