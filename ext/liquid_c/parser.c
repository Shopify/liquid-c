#include "liquid.h"
#include "parser.h"
#include "lexer.h"

static VALUE cLiquidRangeLookup, cLiquidVariableLookup, cRange, vLiquidExpressionLiterals;
static ID idToI, idEvaluate;

void init_parser(parser_t *p, const char *str, const char *end)
{
    p->str_end = end;
    p->cur.type = p->next.type = TOKEN_EOS;
    p->str = lex_one(str, end, &p->cur);
    p->str = lex_one(p->str, end, &p->next);
}

lexer_token_t parser_consume_any(parser_t *p)
{
    lexer_token_t cur = p->cur;
    p->cur = p->next;
    p->next.type = TOKEN_EOS;
    p->str = lex_one(p->str, p->str_end, &p->next);
    return cur;
}

lexer_token_t parser_must_consume(parser_t *p, unsigned char type)
{
    if (p->cur.type != type) {
        rb_enc_raise(utf8_encoding, cLiquidSyntaxError, "Expected %s but found %s",
                 symbol_names[type], symbol_names[p->cur.type]);
    }
    return parser_consume_any(p);
}

lexer_token_t parser_consume(parser_t *p, unsigned char type)
{
    if (p->cur.type != type) {
        lexer_token_t zero = {0};
        return zero;
    }
    return parser_consume_any(p);
}

inline static int rstring_eq(VALUE rstr, const char *str) {
    size_t str_len = strlen(str);

    return TYPE(rstr) == T_STRING &&
           str_len == (size_t)RSTRING_LEN(rstr) &&
           memcmp(RSTRING_PTR(rstr), str, str_len) == 0;
}

static VALUE parse_number(parser_t *p)
{
    VALUE out;
    lexer_token_t token = parser_must_consume(p, TOKEN_NUMBER);

    // Set up sentinel for rb_cstr operations.
    char tmp = *token.val_end;
    *(char *)token.val_end = '\0';

    if (token.flags & TOKEN_FLOAT_NUMBER) {
        out = DBL2NUM(rb_cstr_to_dbl(token.val, 1));
    } else {
        out = rb_cstr_to_inum(token.val, 10, 1);
    }

    *(char *)token.val_end = tmp;
    return out;
}

static VALUE parse_range(parser_t *p)
{
    parser_must_consume(p, TOKEN_OPEN_ROUND);

    VALUE args[2];
    args[0] = parse_expression(p);
    parser_must_consume(p, TOKEN_DOTDOT);

    args[1] = parse_expression(p);
    parser_must_consume(p, TOKEN_CLOSE_ROUND);

    if (rb_respond_to(args[0], idEvaluate) || rb_respond_to(args[1], idEvaluate))
        return rb_class_new_instance(2, args, cLiquidRangeLookup);

    return rb_class_new_instance(2, args, cRange);
}

static VALUE parse_variable(parser_t *p)
{
    VALUE name, lookups = rb_ary_new(), lookup;
    unsigned long long command_flags = 0;

    if (parser_consume(p, TOKEN_OPEN_SQUARE).type) {
        name = parse_expression(p);
        parser_must_consume(p, TOKEN_CLOSE_SQUARE);
    } else {
        name = token_to_rstr(parser_must_consume(p, TOKEN_IDENTIFIER));
    }

    while (true) {
        if (p->cur.type == TOKEN_OPEN_SQUARE) {
            parser_consume_any(p);
            lookup = parse_expression(p);
            parser_must_consume(p, TOKEN_CLOSE_SQUARE);

            rb_ary_push(lookups, lookup);
        } else if (p->cur.type == TOKEN_DOT) {
            int has_space_affix = parser_consume_any(p).flags & TOKEN_SPACE_AFFIX;
            lookup = token_to_rstr(parser_must_consume(p, TOKEN_IDENTIFIER));

            if (has_space_affix)
                rb_enc_raise(utf8_encoding, cLiquidSyntaxError, "Unexpected dot");

            if (rstring_eq(lookup, "size") || rstring_eq(lookup, "first") || rstring_eq(lookup, "last"))
                command_flags |= 1 << RARRAY_LEN(lookups);

            rb_ary_push(lookups, lookup);
        } else {
            break;
        }
    }

    if (RARRAY_LEN(lookups) == 0) {
        VALUE literal = rb_hash_lookup2(vLiquidExpressionLiterals, name, Qundef);
        if (literal != Qundef) return literal;
    }

    VALUE args[4] = {Qfalse, name, lookups, INT2FIX(command_flags)};
    return rb_class_new_instance(4, args, cLiquidVariableLookup);
}

VALUE parse_expression(parser_t *p)
{
    switch (p->cur.type) {
        case TOKEN_IDENTIFIER:
        case TOKEN_OPEN_SQUARE:
            return parse_variable(p);

        case TOKEN_NUMBER:
            return parse_number(p);

        case TOKEN_OPEN_ROUND:
            return parse_range(p);

        case TOKEN_STRING:
        {
            lexer_token_t token = parser_consume_any(p);
            token.val++;
            token.val_end--;
            return token_to_rstr(token);
        }
    }

    if (p->cur.type == TOKEN_EOS) {
        rb_enc_raise(utf8_encoding, cLiquidSyntaxError, "[:%s] is not a valid expression", symbol_names[p->cur.type]);
    } else {
        rb_enc_raise(utf8_encoding, cLiquidSyntaxError, "[:%s, \"%.*s\"] is not a valid expression",
                 symbol_names[p->cur.type], (int)(p->cur.val_end - p->cur.val), p->cur.val);
    }
    return Qnil;
}

static VALUE rb_parse_expression(VALUE self, VALUE markup)
{
    StringValue(markup);
    char *start = RSTRING_PTR(markup);

    parser_t p;
    init_parser(&p, start, start + RSTRING_LEN(markup));

    if (p.cur.type == TOKEN_EOS)
        return Qnil;

    VALUE expr = parse_expression(&p);

    if (p.cur.type != TOKEN_EOS)
        rb_enc_raise(utf8_encoding, cLiquidSyntaxError, "[:%s] is not a valid expression", symbol_names[p.cur.type]);

    return expr;
}

void init_liquid_parser(void)
{
    idToI = rb_intern("to_i");
    idEvaluate = rb_intern("evaluate");

    cLiquidRangeLookup = rb_const_get(mLiquid, rb_intern("RangeLookup"));
    cRange = rb_const_get(rb_cObject, rb_intern("Range"));
    cLiquidVariableLookup = rb_const_get(mLiquid, rb_intern("VariableLookup"));

    VALUE cLiquidExpression = rb_const_get(mLiquid, rb_intern("Expression"));
    rb_define_singleton_method(cLiquidExpression, "c_parse", rb_parse_expression, 1);

    vLiquidExpressionLiterals = rb_const_get(cLiquidExpression, rb_intern("LITERALS"));
}

