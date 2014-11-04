#include "liquid.h"
#include "variable.h"
#include "parser.h"
#include <stdio.h>

static VALUE rb_variable_parse(VALUE self, VALUE markup, VALUE filters)
{
    StringValue(markup);
    char *start = RSTRING_PTR(markup);

    parser_t p;
    init_parser(&p, start, start + RSTRING_LEN(markup));

    if (p.cur.type == TOKEN_EOS)
        return Qnil;

    VALUE name = parse_expression(&p);

    while (parser_consume(&p, TOKEN_PIPE).type) {
        lexer_token_t filter_name = parser_must_consume(&p, TOKEN_IDENTIFIER);

        VALUE filter_args = rb_ary_new(), keyword_args = Qnil, filter;

        if (parser_consume(&p, TOKEN_COLON).type) {
            do {
                if (p.cur.type == TOKEN_IDENTIFIER && p.next.type == TOKEN_COLON) {
                    VALUE key = token_to_rstr(parser_consume_any(&p));
                    parser_consume_any(&p);

                    if (keyword_args == Qnil) keyword_args = rb_hash_new();
                    rb_hash_aset(keyword_args, key, parse_expression(&p));
                } else {
                    rb_ary_push(filter_args, parse_expression(&p));
                }
            } while (parser_consume(&p, TOKEN_COMMA).type);
        }

        if (keyword_args == Qnil) {
            filter = rb_ary_new3(2, token_to_rstr(filter_name), filter_args);
        } else {
            filter = rb_ary_new3(3, token_to_rstr(filter_name), filter_args, keyword_args);
        }
        rb_ary_push(filters, filter);
    }

    parser_must_consume(&p, TOKEN_EOS);
    return name;
}

void init_liquid_variable(void)
{
    rb_define_singleton_method(cLiquidVariable, "c_strict_parse", rb_variable_parse, 2);
}

