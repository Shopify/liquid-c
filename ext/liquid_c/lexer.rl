#include "liquid.h"
#include "lexer.h"
#include "tokenizer.h"
#include <stdio.h>

typedef struct lexer_token {
    unsigned char type;
    char *val;
    unsigned int val_len;
} lexer_token_t;

static const char *symbol_names[256] = {
    [TOKEN_COMPARISON] = "comparison",
    [TOKEN_QUOTE] = "string",
    [TOKEN_NUMBER] = "number",
    [TOKEN_IDENTIFIER] = "id",
    [TOKEN_DOTDOT] = "dotdot",
    [TOKEN_EOS] = "end_of_string",
    ['|'] = "pipe",
    ['.'] = "dot",
    [':'] = "colon",
    [','] = "comma",
    ['['] = "open_square",
    [']'] = "close_square",
    ['('] = "open_round",
    [')'] = "close_round"
};

VALUE symbols[256] = {0};

VALUE get_rb_type(unsigned char type) {
    VALUE ret = symbols[type];
    if (ret) return ret;

    ret = ID2SYM(rb_intern(symbol_names[type]));
    symbols[type] = ret;
    return ret;
}

VALUE cLiquidSyntaxError;

#define PUSH_TOKEN(type) \
    { \
        VALUE rb_token = rb_ary_new(); \
        rb_ary_push(rb_token, get_rb_type(type)); \
        rb_ary_push(rb_token, rb_str_new(ts, te - ts)); \
        rb_ary_push(tokens, rb_token); \
    }

%%{
    machine liquid_lexer;

    escape_char = "\\" any;
    dquoted_char = (^("\"" | "\\") | escape_char);
    squoted_char = (^("'" | "\\") | escape_char);
    quote = ("\"" dquoted_char* "\"" | "'" squoted_char* "'");

    comparison = ('=='|'!='|'<>'|'<='|'>='|'<'|'>'|'contains');
    dotdot = '..';
    number = '-'? digit+ ('.' digit+)?;
    identifier = [0-9A-Za-z_\-?!]+;
    specials = ('|'|'.'|':'|','|'['|']'|'|'|'('|')');

    main := |*
        space;
        comparison => { PUSH_TOKEN(TOKEN_COMPARISON); };
        quote      => { PUSH_TOKEN(TOKEN_QUOTE); };
        number     => { PUSH_TOKEN(TOKEN_NUMBER); };
        identifier => { PUSH_TOKEN(TOKEN_IDENTIFIER); };
        dotdot     => { PUSH_TOKEN(TOKEN_DOTDOT); };
        specials   => { PUSH_TOKEN(*ts); };
    *|;
}%%

%% write data;

static VALUE lex(VALUE self, VALUE tokens, VALUE rb_input)
{
    fprintf(stderr, "Y");
    unsigned int len = RSTRING_LEN(rb_input);
    char *str = RSTRING_PTR(rb_input);

    int cs, act;
    char *p = str, *ts, *te;
    char *pe = str + len + 1;
    char *eof = pe;

    %% write init;
    %% write exec;

    if (p < eof - 1) {
        rb_raise(cLiquidSyntaxError, "Unexpected character %c", *ts);
        return Qnil;
    }

    VALUE rb_eof = rb_ary_new();
    rb_ary_push(rb_eof, get_rb_type(TOKEN_EOS));
    rb_ary_push(tokens, rb_eof);

    return tokens;
}

void init_liquid_lexer(void)
{
    cLiquidSyntaxError = rb_const_get(mLiquid, rb_intern("SyntaxError"));
    rb_define_singleton_method(mLiquid, "c_lex", lex, 2);
}
