#if !defined(LIQUID_LEXER_H)
#define LIQUID_LEXER_H

enum lexer_token_type {
    TOKEN_NONE,
    TOKEN_COMPARISON,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_IDENTIFIER,
    TOKEN_DOTDOT,
    TOKEN_EOS,

    TOKEN_PIPE = '|',
    TOKEN_DOT = '.',
    TOKEN_COLON = ':',
    TOKEN_COMMA = ',',
    TOKEN_OPEN_SQUARE = '[',
    TOKEN_CLOSE_SQUARE = ']',
    TOKEN_OPEN_ROUND = '(',
    TOKEN_CLOSE_ROUND = ')',
    TOKEN_QUESTION = '?',
    TOKEN_DASH = '-',

    TOKEN_END = 256
};

typedef struct lexer_token {
    unsigned char type;
    const char *val, *val_end;
} lexer_token_t;

extern const char *symbol_names[TOKEN_END];

const char *lex_one(const char *str, const char *end, lexer_token_t *token);
VALUE rb_lex(VALUE self, VALUE markup);
void init_liquid_lexer(void);

#endif

