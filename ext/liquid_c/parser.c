#include "liquid.h"
#include "parser.h"
#include "lexer.h"

static VALUE cLiquidRangeLookup, cLiquidVariableLookup, cRange, symBlank, symEmpty;
static ID idToI, idParse, idEvaluate;

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

inline static int rstring_eq(VALUE rstr, const char *str) {
    size_t str_len = strlen(str);

    if (str_len != (size_t)RSTRING_LEN(rstr)) return 0;
    return memcmp(RSTRING_PTR(rstr), str, str_len) == 0;
}

static VALUE parse_number(parser_t *parser)
{
    VALUE out;
    lexer_token_t token = parser_must_consume(parser, TOKEN_NUMBER);

    // Set up sentinel for rb_cstr operations.
    char tmp = *token.val_end;
    *(char *)token.val_end = '\0';

    if (memchr(token.val, '.', token.val_end - token.val)) {
        // Float.
        out = DBL2NUM(rb_cstr_to_dbl(token.val, 1));
    } else {
        // Integer.
        out = rb_cstr_to_inum(token.val, 10, 1);
    }

    *(char *)token.val_end = tmp;
    return out;
}

static VALUE parse_range(parser_t *parser)
{
    parser_must_consume(parser, TOKEN_OPEN_ROUND);

    VALUE args[2];
    args[0] = parse_expression(parser);
    parser_must_consume(parser, TOKEN_DOTDOT);

    args[1] = parse_expression(parser);
    parser_must_consume(parser, TOKEN_CLOSE_ROUND);

    if (rb_respond_to(args[0], idEvaluate) || rb_respond_to(args[1], idEvaluate))
        return rb_class_new_instance(2, args, cLiquidRangeLookup);

    return rb_class_new_instance(2, args, cRange);
}

static VALUE parse_variable_name(parser_t *parser)
{
    VALUE name;

    if (parser_consume(parser, TOKEN_OPEN_SQUARE).type) {
        name = parse_expression(parser);
        parser_must_consume(parser, TOKEN_CLOSE_SQUARE);
        return name;
    }

    lexer_token_t token = parser_must_consume(parser, TOKEN_IDENTIFIER);
    const char *start = token.val, *end = token.val_end;

    while (parser_consume(parser, TOKEN_DASH).type)
        end = parser_must_consume(parser, TOKEN_IDENTIFIER).val_end;

    if (parser->cur.type == TOKEN_QUESTION)
        end = parser_consume_any(parser).val_end;

    return rb_enc_str_new(start, end - start, utf8_encoding);
}

#define LOOKUP_EQ(str) value_equals(parser->cur, (str))

static VALUE parse_variable(parser_t *parser)
{
    VALUE name = parse_variable_name(parser);

    VALUE lookups = rb_ary_new(), lookup;
    unsigned long long command_flags = 0;
    unsigned char last_type, cur_type = 0;
    int must_have_id = 0;

    while (true) {
        last_type = cur_type;
        cur_type = parser->cur.type;

        if (must_have_id && cur_type != TOKEN_IDENTIFIER)
            parser_must_consume(parser, TOKEN_IDENTIFIER);

        switch (cur_type) {
            case TOKEN_IDENTIFIER:
                must_have_id = 0;
                if (last_type != TOKEN_DOT) break;
                lookup = parse_variable_name(parser);

                if (rstring_eq(lookup, "size") || rstring_eq(lookup, "first") || rstring_eq(lookup, "last"))
                    command_flags |= 1 << RARRAY_LEN(lookups);

                rb_ary_push(lookups, lookup);
                continue;
            case TOKEN_OPEN_SQUARE:
                if (last_type == TOKEN_DOT) break;
                rb_ary_push(lookups, parse_variable_name(parser));
                continue;
            case TOKEN_DOT:
                must_have_id = 1;
                parser_consume_any(parser);
                continue;
        }
        break;
    }

    if (RARRAY_LEN(lookups) == 0) {
        if (RSTRING_LEN(name) == 0 || rstring_eq(name, "nil") || rstring_eq(name, "null")) return Qnil;
        else if (rstring_eq(name, "true")) return Qtrue;
        else if (rstring_eq(name, "false")) return Qfalse;
        else if (rstring_eq(name, "blank")) return symBlank;
        else if (rstring_eq(name, "empty")) return symEmpty;
    }

    VALUE args[4] = {Qfalse, name, lookups, INT2FIX(command_flags)};
    return rb_class_new_instance(4, args, cLiquidVariableLookup);
}

VALUE parse_expression(parser_t *parser)
{
    switch (parser->cur.type) {
        case TOKEN_IDENTIFIER:
        case TOKEN_OPEN_SQUARE:
            return parse_variable(parser);

        case TOKEN_NUMBER:
            return parse_number(parser);

        case TOKEN_OPEN_ROUND:
            return parse_range(parser);

        case TOKEN_QUOTE:
        {
            lexer_token_t token = parser_consume_any(parser);
            token.val++;
            token.val_end--;
            return TOKEN_STR(token);
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

void init_liquid_parser(void)
{
    idToI = rb_intern("to_i");
    idParse = rb_intern("parse");
    idEvaluate = rb_intern("evaluate");
    symBlank = ID2SYM(rb_intern("blank?"));
    symEmpty = ID2SYM(rb_intern("empty?"));

    cLiquidRangeLookup = rb_const_get(mLiquid, rb_intern("RangeLookup"));
    cRange = rb_const_get(rb_cObject, rb_intern("Range"));
    cLiquidVariableLookup = rb_const_get(mLiquid, rb_intern("VariableLookup"));
}

