
#include "liquid.h"
#include "variable.h"
#include "parser.h"
#include <stdio.h>

static VALUE cLiquidVariable, cLiquidExpression;
static ID idParse;

VALUE rb_variable_parse(VALUE self, VALUE markup)
{
    StringValue(markup);
    char *start = RSTRING_PTR(markup);

    parser_t p;
    init_parser(&p, start, start + RSTRING_LEN(markup));

    VALUE filters = rb_ary_new();

    if (p.cur.type == TOKEN_EOS)
        return rb_ary_new3(2, Qnil, filters);

    VALUE name = parse_expression(&p);

    while (parser_consume(&p, TOKEN_PIPE).type) {
        lexer_token_t filter_name = parser_must_consume(&p, TOKEN_IDENTIFIER);

        VALUE filter_args = rb_ary_new(),
              keyword_args = rb_hash_new();

        if (parser_consume(&p, TOKEN_COLON).type) {
            do {
                if (p.cur.type == TOKEN_IDENTIFIER && p.next.type == TOKEN_COLON) {
                    lexer_token_t key_token = parser_consume_any(&p);
                    parser_consume_any(&p);
                    rb_hash_aset(keyword_args, TOKEN_TO_RSTR(key_token), parse_expression(&p));
                } else {
                    rb_ary_push(filter_args, parse_expression(&p));
                }
            }
            while (parser_consume(&p, TOKEN_COMMA).type);
        }

        VALUE filter = rb_ary_new3(2, TOKEN_TO_RSTR(filter_name), filter_args);

        if (RHASH_SIZE(keyword_args))
            rb_ary_push(filter, keyword_args);

        rb_ary_push(filters, filter);
    }

    parser_must_consume(&p, TOKEN_EOS);
    return rb_ary_new3(2, name, filters);
}

void init_liquid_variable(void)
{
    idParse = rb_intern("parse");
    cLiquidVariable = rb_const_get(mLiquid, rb_intern("Variable"));
    cLiquidExpression = rb_const_get(mLiquid, rb_intern("Expression"));
    rb_define_singleton_method(cLiquidVariable, "c_strict_parse", rb_variable_parse, 1);
}

