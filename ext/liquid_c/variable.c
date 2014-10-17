
#include "liquid.h"
#include "variable.h"
#include "parser.h"
#include <stdio.h>

static VALUE cLiquidVariable, cLiquidExpression;
static ID iParse;

VALUE rb_variable_parse(VALUE self, VALUE markup)
{
    StringValue(markup);

    parser_t parser;
    char *start = RSTRING_PTR(markup);
    init_parser(&parser, start, start + RSTRING_LEN(markup));

    VALUE name, filters = rb_ary_new();

    if (parser.cur.type == TOKEN_EOS) {
        return rb_ary_new3(2, Qnil, filters);
    } else if (parser.cur.type == TOKEN_PIPE) {
        name = Qnil;
    } else {
        name = rb_funcall(cLiquidExpression, iParse, 1, parse_expression(&parser));
    }

    while (parser_consume(&parser, TOKEN_PIPE).type) {
        lexer_token_t filter_name = parser_must_consume(&parser, TOKEN_IDENTIFIER);

        VALUE filter_args = rb_ary_new(), keyword_args = rb_hash_new();

        if (parser_consume(&parser, TOKEN_COLON).type) {
            do {
                VALUE value;

                if (parser.cur.type == TOKEN_IDENTIFIER && parser.next.type == TOKEN_COLON) {
                    lexer_token_t key_token = parser_consume_any(&parser);
                    parser_consume_any(&parser);

                    value = rb_funcall(cLiquidExpression, iParse, 1, parse_expression(&parser));
                    rb_hash_aset(keyword_args, TOKEN_STR(key_token), value);
                } else {
                    value = rb_funcall(cLiquidExpression, iParse, 1, parse_expression(&parser));
                    rb_ary_push(filter_args, value);
                }
            }
            while (parser_consume(&parser, TOKEN_COMMA).type);
        }

        VALUE filter = rb_ary_new3(2, TOKEN_STR(filter_name), filter_args);
        if (RHASH_SIZE(keyword_args)) {
            rb_ary_push(filter, keyword_args);
        }
        rb_ary_push(filters, filter);
    }

    parser_must_consume(&parser, TOKEN_EOS);
    return rb_ary_new3(2, name, filters);
}

void init_liquid_variable(void)
{
    iParse = rb_intern("parse");
    cLiquidVariable = rb_const_get(mLiquid, rb_intern("Variable"));
    cLiquidExpression = rb_const_get(mLiquid, rb_intern("Expression"));
    rb_define_singleton_method(cLiquidVariable, "c_strict_parse", rb_variable_parse, 1);
}

