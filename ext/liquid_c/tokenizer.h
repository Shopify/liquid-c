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

    // str_trimmed contains no tag delimiters
    const char *str_trimmed, *str_full;
    long len_trimmed, len_full;

    bool lstrip, rstrip;
} token_t;

typedef struct tokenizer {
    VALUE source;
    const char *cursor, *cursor_end;
    unsigned int line_number;
    bool lstrip_flag;
    bool for_liquid_tag;
    bool bug_compatible_whitespace_trimming;
} tokenizer_t;

extern VALUE cLiquidTokenizer;
extern const rb_data_type_t tokenizer_data_type;
#define Tokenizer_Get_Struct(obj, sval) TypedData_Get_Struct(obj, tokenizer_t, &tokenizer_data_type, sval)

void init_liquid_tokenizer();
void tokenizer_next(tokenizer_t *tokenizer, token_t *token);

VALUE tokenizer_new_from_cstr(VALUE source, const char *cursor, const char *cursor_end, int line_number, bool for_liquid_tag);

#endif

