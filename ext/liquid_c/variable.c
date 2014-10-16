
#include "liquid.h"
#include "variable.h"
#include "lexer.h"
#include <stdio.h>

VALUE rb_variable_parse(VALUE self, VALUE markup, VALUE filters)
{
    StringValue(markup);
    Check_Type(filters, T_ARRAY);

    lexer_t lexer;
    char *start = RSTRING_PTR(markup);
    init_lexer(&lexer, start, start + RSTRING_LEN(markup));

    VALUE name;

    if (lexer.cur.type == TOKEN_EOS) {
        return Qnil;
    } else if (lexer.cur.type == TOKEN_PIPE) {
        name = rb_str_new2("");
    } else {
        name = lexer_expression(&lexer);
    }

    while (lexer_consume(&lexer, TOKEN_PIPE).type) {
        lexer_token_t filter_name = lexer_must_consume(&lexer, TOKEN_IDENTIFIER);

        VALUE filter_args = rb_ary_new();

        if (lexer_consume(&lexer, TOKEN_COLON).type) {
            rb_ary_push(filter_args, lexer_argument(&lexer));

            while (lexer_consume(&lexer, TOKEN_COMMA).type) {
                rb_ary_push(filter_args, lexer_argument(&lexer));
            }
        }

        rb_ary_push(filters, rb_ary_new3(2, rb_utf8_str_new_range(filter_name.val, filter_name.val_end), filter_args));
    }

    lexer_must_consume(&lexer, TOKEN_EOS);
    return name;
}

void init_liquid_variable(void)
{
    rb_define_singleton_method(mLiquid, "c_variable_parse", rb_variable_parse, 2);
}
