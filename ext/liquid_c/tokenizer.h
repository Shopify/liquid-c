#if !defined(LIQUID_TOKENIZER_H)
#define LIQUID_TOKENIZER_H

enum token_type {
    TOKEN_NONE,
    TOKEN_INVALID,
    TOKEN_STRING,
    TOKEN_TAG,
    TOKEN_VARIABLE
};

typedef struct token {
    enum token_type type;
    const char *str;
    long length;
} token_t;

typedef struct tokenizer {
    VALUE source;
    const char *cursor;
    bool use_line_numbers;

    long length;
    long current_line_number;
} tokenizer_t;

extern VALUE cLiquidTokenizer;
extern const rb_data_type_t tokenizer_data_type;
#define Tokenizer_Get_Struct(obj, sval) TypedData_Get_Struct(obj, tokenizer_t, &tokenizer_data_type, sval)

void init_liquid_tokenizer();
void tokenizer_next(tokenizer_t *tokenizer, token_t *token);

#endif
