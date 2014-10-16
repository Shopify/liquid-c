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

static const char *symbol_names[256] = {
    [TOKEN_COMPARISON] = "comparison",
    [TOKEN_QUOTE] = "string",
    [TOKEN_NUMBER] = "number",
    [TOKEN_IDENTIFIER] = "id",
    [TOKEN_DOTDOT] = "dotdot",
    [TOKEN_EOS] = "end_of_string",
    [TOKEN_PIPE] = "pipe",
    [TOKEN_DOT] = "dot",
    [TOKEN_COLON] = "colon",
    [TOKEN_COMMA] = "comma",
    [TOKEN_OPEN_SQUARE] = "open_square",
    [TOKEN_CLOSE_SQUARE] = "close_square",
    [TOKEN_OPEN_ROUND] = "open_round",
    [TOKEN_CLOSE_ROUND] = "close_round"
};

typedef struct lexer_token {
    unsigned char type;
    const char *val;
    unsigned int val_len;
} lexer_token_t;

typedef struct lexer_token_list {
    lexer_token_t *list;
    unsigned int len, cap;
} lexer_token_list_t;

void lex(const char *start, const char *end, lexer_token_list_t *tokens);
VALUE rb_lex(VALUE self, VALUE markup);
lexer_token_t *consume(lexer_token_list_t *tokens, unsigned char type);
lexer_token_list_t *new_token_list();
void init_liquid_lexer(void);

#endif

