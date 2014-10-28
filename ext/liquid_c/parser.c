#include "liquid.h"
#include "parser.h"
#include "lexer.h"

static VALUE cLiquidRangeLookup, cLiquidVariableLookup, cRange, symBlank, symEmpty;
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
        rb_raise(cLiquidSyntaxError, "Expected %s but found %s",
                 symbol_names[type], symbol_names[p->cur.type]);
    }
    return parser_consume_any(p);
}

lexer_token_t parser_consume(parser_t *p, unsigned char type)
{
    if (p->cur.type != type) {
        lexer_token_t zero;
        zero.type = 0;
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

static VALUE parse_variable_name(parser_t *p)
{
    VALUE name;

    if (parser_consume(p, TOKEN_OPEN_SQUARE).type) {
        name = parse_expression(p);
        parser_must_consume(p, TOKEN_CLOSE_SQUARE);
        return name;
    }

    lexer_token_t token = parser_must_consume(p, TOKEN_IDENTIFIER);
    const char *start = token.val, *end = token.val_end;

    while (p->cur.type == TOKEN_DASH) {
        if (parser_consume_any(p).flags & TOKEN_SPACE_AFFIX) {
            // A common mistake is to assume that math syntax works in Liquid.
            // This leads people to attempting to subtract variables, i.e. 'a - b'
            // Liquid allows dashes in variable names, and since the C lexer ignores
            // whitespace, this case must be handled explicitly to prevent a variable
            // with name 'a - b' from being parsed.
            rb_raise(cLiquidSyntaxError, "Unexpected dash");
        }

        end = parser_must_consume(p, TOKEN_IDENTIFIER).val_end;
    }

    if (p->cur.type == TOKEN_QUESTION)
        end = parser_consume_any(p).val_end;

    return rb_enc_str_new(start, end - start, utf8_encoding);
}

static VALUE parse_variable(parser_t *p)
{
    VALUE name = parse_variable_name(p);

    VALUE lookups = rb_ary_new(), lookup;
    unsigned long long command_flags = 0;
    unsigned char last_type, cur_type = 0;
    int must_have_id = 0;

    while (true) {
        last_type = cur_type;
        cur_type = p->cur.type;

        if (must_have_id && cur_type != TOKEN_IDENTIFIER)
            parser_must_consume(p, TOKEN_IDENTIFIER);

        switch (cur_type) {
            case TOKEN_IDENTIFIER:
                must_have_id = 0;
                if (last_type != TOKEN_DOT) break;
                lookup = parse_variable_name(p);

                if (rstring_eq(lookup, "size") || rstring_eq(lookup, "first") || rstring_eq(lookup, "last"))
                    command_flags |= 1 << RARRAY_LEN(lookups);

                rb_ary_push(lookups, lookup);
                continue;
            case TOKEN_OPEN_SQUARE:
                if (last_type == TOKEN_DOT) break;
                rb_ary_push(lookups, parse_variable_name(p));
                continue;
            case TOKEN_DOT:
                must_have_id = 1;
                if (parser_consume_any(p).flags & TOKEN_SPACE_AFFIX) {
                    parser_must_consume(p, TOKEN_IDENTIFIER);
                    rb_raise(cLiquidSyntaxError, "Unexpected dot");
                }
                continue;
        }
        break;
    }

    if (RARRAY_LEN(lookups) == 0 && TYPE(name) == T_STRING) {
        if (rstring_eq(name, "nil") || rstring_eq(name, "null")) return Qnil;
        if (rstring_eq(name, "true")) return Qtrue;
        if (rstring_eq(name, "false")) return Qfalse;
        if (rstring_eq(name, "blank")) return symBlank;
        if (rstring_eq(name, "empty")) return symEmpty;
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
            return TOKEN_TO_RSTR(token);
        }
    }

    if (p->cur.type != TOKEN_EOS) {
        rb_raise(cLiquidSyntaxError, "[:%s] is not a valid expression", symbol_names[p->cur.type]);
    } else {
        rb_raise(cLiquidSyntaxError, "[:%s, \"%.*s\"] is not a valid expression",
                 symbol_names[p->cur.type], (int)(p->cur.val_end - p->cur.val), p->cur.val);
    }
    return Qnil;
}

void init_liquid_parser(void)
{
    idToI = rb_intern("to_i");
    idEvaluate = rb_intern("evaluate");
    symBlank = ID2SYM(rb_intern("blank?"));
    symEmpty = ID2SYM(rb_intern("empty?"));

    cLiquidRangeLookup = rb_const_get(mLiquid, rb_intern("RangeLookup"));
    cRange = rb_const_get(rb_cObject, rb_intern("Range"));
    cLiquidVariableLookup = rb_const_get(mLiquid, rb_intern("VariableLookup"));
}

