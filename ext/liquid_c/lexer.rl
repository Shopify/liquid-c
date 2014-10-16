#include "liquid.h"
#include "lexer.h"
#include "tokenizer.h"
#include <stdio.h>

VALUE symbols[256] = {0};

VALUE get_rb_type(unsigned char type) {
    VALUE ret = symbols[type];
    if (ret) return ret;

    ret = ID2SYM(rb_intern(symbol_names[type]));
    symbols[type] = ret;
    return ret;
}

VALUE cLiquidSyntaxError;

#define PUSH_TOKEN(type) append_token(tokens, type, ts, te - ts);

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

void append_token(lexer_token_list_t *tokens, unsigned char type, const char *val, unsigned int val_len) {
    if (tokens->len >= tokens->cap) {
        tokens->cap <<= 1;
        tokens->list = realloc(tokens->list, tokens->cap * sizeof(lexer_token_t));
    }

    lexer_token_t token;
    fprintf(stderr, "[%s]\n", symbol_names[type]);
    token.type = type;
    token.val = val;
    token.val_len = val_len;
    tokens->list[tokens->len++] = token;
}

void lex(const char *start, const char *end, lexer_token_list_t *tokens)
{
    // Ragel variables.
    int cs, act;
    const char *p = start, *pe = end, *eof = end;
    const char *ts, *te;

    %% write init;
    %% write exec;

    if (p < eof - 1) {
        rb_raise(cLiquidSyntaxError, "Unexpected character %c", *ts);
    }

    append_token(tokens, TOKEN_EOS, NULL, 0);
}

lexer_token_t *consume(lexer_token_list_t *tokens, unsigned char type)
{
    if (tokens->len <= 0) {
        rb_raise(cLiquidSyntaxError, "Expected %s but found nothing", symbol_names[type]);
    }

    lexer_token_t *token = tokens->list++;
    if (token->type != type) {
        rb_raise(cLiquidSyntaxError, "Expected %s but found %s", symbol_names[type], symbol_names[token->type]);
    }

    return token;
}

lexer_token_list_t *new_token_list()
{
    lexer_token_list_t *tokens = malloc(sizeof(lexer_token_list_t));
    tokens->len = 0;
    tokens->cap = 4;
    tokens->list = malloc(4 * sizeof(lexer_token_t));
    return tokens;
}

void init_liquid_lexer(void)
{
    cLiquidSyntaxError = rb_const_get(mLiquid, rb_intern("SyntaxError"));
    rb_define_singleton_method(mLiquid, "c_lex", lex, 2);
}
