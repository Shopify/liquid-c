
#include "liquid.h"
#include "variable.h"
#include "parser.h"
#include <stdio.h>

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
        name = rb_str_new2("");
    } else {
        name = parse_expression(&parser);
    }

    while (parser_consume(&parser, TOKEN_PIPE).type) {
        lexer_token_t filter_name = parser_must_consume(&parser, TOKEN_IDENTIFIER);

        VALUE filter_args = rb_ary_new();

        if (parser_consume(&parser, TOKEN_COLON).type) {
            rb_ary_push(filter_args, parse_argument(&parser));

            while (parser_consume(&parser, TOKEN_COMMA).type) {
                rb_ary_push(filter_args, parse_argument(&parser));
            }
        }

        rb_ary_push(filters, rb_ary_new3(2, rb_utf8_str_new_range(filter_name.val, filter_name.val_end), filter_args));
    }

    parser_must_consume(&parser, TOKEN_EOS);
    return rb_ary_new3(2, name, filters);
}

static VALUE cLiquidVariable;

void init_liquid_variable(void)
{
    cLiquidVariable = rb_const_get(mLiquid, rb_intern("Variable"));
    rb_define_singleton_method(cLiquidVariable, "c_strict_parse", rb_variable_parse, 1);
}
