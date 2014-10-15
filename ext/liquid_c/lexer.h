#if !defined(LIQUID_LEXER_H)
#define LIQUID_LEXER_H

enum lexer_token_type {
    TOKEN_COMPARISON = 1,
    TOKEN_QUOTE,
    TOKEN_NUMBER,
    TOKEN_IDENTIFIER,
    TOKEN_DOTDOT,
    TOKEN_EOS
};

void init_liquid_lexer(void);

#endif

