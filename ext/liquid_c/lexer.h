#if !defined(LIQUID_LEXER_H)
#define LIQUID_LEXER_H

enum lexer_token_type {
    TOKEN_COMPARISON = 1,
    TOKEN_QUOTE,
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
    TOKEN_CLOSE_ROUND = ')'
};

extern const char *symbol_names[256];

typedef struct lexer_token {
    unsigned char type;
    const char *val, *val_end;
} lexer_token_t;

typedef struct lexer {
    lexer_token_t cur, next;
    const char *str, *str_end;
} lexer_t;

void init_lexer(lexer_t *lexer, const char *str, const char *end);

lexer_token_t lexer_must_consume(lexer_t *lexer, unsigned char type);
lexer_token_t lexer_consume(lexer_t *lexer, unsigned char type);
lexer_token_t lexer_consume_any(lexer_t *lexer);

VALUE lexer_expression(lexer_t *lexer);
VALUE lexer_argument(lexer_t *lexer);
VALUE lexer_variable_signature(lexer_t *lexer);

VALUE rb_lex(VALUE self, VALUE markup);
void init_liquid_lexer(void);

#endif

