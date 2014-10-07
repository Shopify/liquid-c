#include "liquid.h"
#include "tokenizer.h"

VALUE cLiquidTokenizer, cLiquidToken;

static void tokenizer_mark(void *ptr)
{
    tokenizer_t *tokenizer = ptr;
    rb_gc_mark(tokenizer->source);
}

static void tokenizer_free(void *ptr)
{
    tokenizer_t *tokenizer = ptr;
    xfree(tokenizer);
}

static size_t tokenizer_memsize(const void *ptr)
{
    return ptr ? sizeof(tokenizer_t) : 0;
}

const rb_data_type_t tokenizer_data_type = {
    "liquid_tokenizer",
    { tokenizer_mark, tokenizer_free, tokenizer_memsize, },
#if defined(RUBY_TYPED_FREE_IMMEDIATELY)
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
#endif
};

static VALUE tokenizer_allocate(VALUE klass)
{
    VALUE obj;
    tokenizer_t *tokenizer;

    obj = TypedData_Make_Struct(klass, tokenizer_t, &tokenizer_data_type, tokenizer);
    tokenizer->source = Qnil;
    return obj;
}

static VALUE tokenizer_initialize_method(int argc, VALUE *argv, VALUE self)
{
    if (argc > 2 || argc == 0)
        rb_raise(rb_eArgError, "wrong number of arguments");

    VALUE source = argv[0];
    tokenizer_t *tokenizer;

    bool use_line_numbers = false;

    if (argc == 2) {
        VALUE options = argv[1];
        Check_Type(options, T_HASH);
        use_line_numbers = RTEST(rb_hash_aref(options, ID2SYM(rb_intern("line_numbers"))));
    }

    Check_Type(source, T_STRING);
    Tokenizer_Get_Struct(self, tokenizer);
    source = rb_str_dup_frozen(source);
    tokenizer->source = source;
    tokenizer->cursor = RSTRING_PTR(source);
    tokenizer->length = RSTRING_LEN(source);
    tokenizer->current_line_number = 1;
    tokenizer->use_line_numbers = use_line_numbers;
    return Qnil;
}

void tokenizer_next(tokenizer_t *tokenizer, token_t *token)
{
    if (tokenizer->length <= 0) {
        memset(token, 0, sizeof(*token));
        return;
    }

    const char *cursor = tokenizer->cursor;
    const char *last = cursor + tokenizer->length - 1;

    token->str = cursor;
    token->type = TOKEN_STRING;

    while (cursor <= last) {
        if (*cursor == '\n')
            tokenizer->current_line_number++;

        if (cursor >= last)
            break;

        if (*cursor++ != '{')
            continue;

        char c = *cursor++;
        if (c != '%' && c != '{')
            continue;
        if (cursor - tokenizer->cursor > 2) {
            token->type = TOKEN_STRING;
            cursor -= 2;
            goto found;
        }
        token->type = TOKEN_INVALID;
        if (c == '%') {
            while (cursor < last) {
                if (*cursor++ != '%')
                    continue;
                c = *cursor++;
                while (c == '%' && cursor <= last)
                    c = *cursor++;
                if (c != '}')
                    continue;
                token->type = TOKEN_TAG;
                goto found;
            }
            // unterminated tag
            cursor = tokenizer->cursor + 2;
            goto found;
        } else {
            while (cursor < last) {
                if (*cursor++ != '}')
                    continue;
                if (*cursor++ != '}') {
                    // variable incomplete end, used to end raw tags
                    cursor--;
                    goto found;
                }
                token->type = TOKEN_VARIABLE;
                goto found;
            }
            // unterminated variable
            cursor = tokenizer->cursor + 2;
            goto found;
        }
    }
    cursor = last + 1;
found:
    token->length = cursor - tokenizer->cursor;
    tokenizer->cursor += token->length;
    tokenizer->length -= token->length;
}

static VALUE tokenizer_shift_method(VALUE self)
{
    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(self, tokenizer);

    token_t token;
    tokenizer_next(tokenizer, &token);
    if (token.type == TOKEN_NONE)
        return Qnil;

    VALUE token_str = rb_enc_str_new(token.str, token.length, utf8_encoding);

    if (tokenizer->use_line_numbers) {
        VALUE line_nr = INT2FIX(tokenizer->current_line_number);
        VALUE args[] = {token_str, line_nr};
        return rb_class_new_instance(2, args, cLiquidToken);
    }
    return token_str;
}

void init_liquid_tokenizer()
{
    cLiquidTokenizer = rb_define_class_under(mLiquid, "Tokenizer", rb_cObject);
    cLiquidToken = rb_const_get(mLiquid, rb_intern("Token"));
    rb_define_alloc_func(cLiquidTokenizer, tokenizer_allocate);
    rb_define_method(cLiquidTokenizer, "initialize", tokenizer_initialize_method, -1);
    rb_define_method(cLiquidTokenizer, "shift", tokenizer_shift_method, 0);
}
