#if !defined(LIQUID_TOKENIZER_H)
#define LIQUID_TOKENIZER_H

enum token_type {
    TOKENIZER_TOKEN_NONE = 0,
    TOKEN_INVALID,
    TOKEN_RAW,
    TOKEN_TAG,
    TOKEN_VARIABLE
};

typedef struct token {
    enum token_type type;
    const char *str;
    long length;
    unsigned int lstrip;
    unsigned int rstrip;
} token_t;

typedef struct tokenizer {
    VALUE source;
    const char *cursor;
    long length;
    unsigned int line_number;
    unsigned int lstrip_flag;
} tokenizer_t;

extern VALUE cLiquidTokenizer;
extern const rb_data_type_t tokenizer_data_type;
#define Tokenizer_Get_Struct(obj, sval) TypedData_Get_Struct(obj, tokenizer_t, &tokenizer_data_type, sval)

void init_liquid_tokenizer();
void tokenizer_next(tokenizer_t *tokenizer, token_t *token);

#endif

