#include "liquid.h"
#include "raw.h"
#include "stringutil.h"
#include "tokenizer.h"

VALUE cLiquidRaw;
static VALUE id_parse_context, id_locale, id_translate, id_block_name, id_block_delimiter, id_ivar_body;

__attribute__((noreturn)) static void raise_translated_syntax_error(VALUE self, const char *key, VALUE args)
{
    VALUE key_str = rb_str_new_cstr(key);
    VALUE msg = rb_funcall(rb_funcall(rb_funcall(self, id_parse_context, 0), id_locale, 0), id_translate, 2, key_str, args);
    rb_raise(cLiquidSyntaxError, "%.*s", (int)RSTRING_LEN(msg), RSTRING_PTR(msg));
}

struct full_token_possibly_invalid_t {
    const char *body_start;
    long body_len;
    const char *delimiter_start;
    long delimiter_len;
};

static bool match_full_token_possibly_invalid(token_t *token, struct full_token_possibly_invalid_t *match)
{
    const char *str = token->str_full;
    long len = token->len_full;

    match->body_start = str;
    match->body_len = 0;
    match->delimiter_start = NULL;
    match->delimiter_len = 0;

    if (len < 5) return false; // Must be at least 5 characters: \{%\w%\}
    if (str[len - 1] != '}' || str[len - 2] != '%') return false;

    const char *curr_delimiter_start;
    long curr_delimiter_len = 0;

    for (long i = len - 3; i >= 0; i--) {
        char c = str[i];

        if (is_word(c)) {
            curr_delimiter_start = str + i;
            curr_delimiter_len++;
        } else {
            if (curr_delimiter_len > 0) {
                match->delimiter_start = curr_delimiter_start;
                match->delimiter_len = curr_delimiter_len;
            }
            curr_delimiter_start = NULL;
            curr_delimiter_len = 0;
        }

        if (c == '%' && match->delimiter_len > 0 &&
                i - 1 >= 0 && str[i - 1] == '{') {
            match->body_len = i - 1;
            return true;
        }
    }

    return false;
}

static VALUE raw_parse_method(VALUE self, VALUE tokens)
{
    c_buffer_t body = c_buffer_allocate(0);

    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(tokens, tokenizer);

    token_t token;
    struct full_token_possibly_invalid_t match;

    VALUE block_delimiter = rb_funcall(self, id_block_delimiter, 0);
    Check_Type(block_delimiter, T_STRING);
    char *block_delimiter_str = RSTRING_PTR(block_delimiter);
    long block_delimiter_len = RSTRING_LEN(block_delimiter);

    while (true) {
        tokenizer_next(tokenizer, &token);

        if (!token.type) break;

        if (match_full_token_possibly_invalid(&token, &match)
                && match.delimiter_len == block_delimiter_len
                && strncmp(match.delimiter_start, block_delimiter_str, block_delimiter_len) == 0) {
            c_buffer_write(&body, (void *)match.body_start, match.body_len);
            VALUE body_str = rb_str_new((char *)body.data,  c_buffer_size(&body));
            rb_ivar_set(self, id_ivar_body, body_str);
            return Qnil;
        }

        c_buffer_write(&body, (char *)token.str_full, token.len_full);
    }

    VALUE hash = rb_hash_new();
    rb_hash_aset(hash, id_block_name, rb_funcall(self, id_block_name, 0));
    raise_translated_syntax_error(self, "errors.syntax.tag_never_closed", hash);
}

void init_liquid_raw()
{
    id_parse_context = rb_intern("parse_context");
    id_locale = rb_intern("locale");
    id_translate = rb_intern("t");
    id_block_name = rb_intern("block_name");
    id_block_delimiter = rb_intern("block_delimiter");
    id_ivar_body = rb_intern("@body");

    cLiquidRaw = rb_const_get(mLiquid, rb_intern("Raw"));
    rb_global_variable(&cLiquidRaw);

    rb_define_method(cLiquidRaw, "c_parse", raw_parse_method, 1);
}
