#include "liquid.h"
#include "tokenizer.h"

VALUE cLiquidTokenizer;

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

static VALUE tokenizer_initialize_method(VALUE self, VALUE source, VALUE line_numbers)
{
    tokenizer_t *tokenizer;

    Check_Type(source, T_STRING);
    Tokenizer_Get_Struct(self, tokenizer);
    source = rb_str_dup_frozen(source);
    tokenizer->source = source;
    tokenizer->cursor = RSTRING_PTR(source);
    tokenizer->length = RSTRING_LEN(source);
    tokenizer->lstrip_flag = 0;
    // tokenizer->line_number keeps track of the current line number or it is 0
    // to indicate that line numbers aren't being calculated
    tokenizer->line_number = RTEST(line_numbers) ? 1 : 0;
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
    token->type = TOKEN_RAW;
    token->lstrip = 0;
    token->rstrip = 0;

    while (cursor < last) {
        if (*cursor++ != '{')
            continue;

        char c = *cursor++;
        if (c != '%' && c != '{')
            continue;
        if (cursor <= last && *cursor == '-') {
          cursor++;
          token->rstrip = 1;
        }
        if (cursor - tokenizer->cursor > (ptrdiff_t)(2 + token->rstrip)) {
            token->type = TOKEN_RAW;
            cursor -= 2 + token->rstrip;
            token->lstrip = tokenizer->lstrip_flag;
            tokenizer->lstrip_flag = 0;
            goto found;
        }
        tokenizer->lstrip_flag = 0;
        token->type = TOKEN_INVALID;
        token->lstrip = token->rstrip;
        token->rstrip = 0;
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
                if(cursor[-3] == '-')
                    token->rstrip = tokenizer->lstrip_flag = 1;
                goto found;
            }
            // unterminated tag
            cursor = tokenizer->cursor + 2;
            tokenizer->lstrip_flag = 0;
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
                if(cursor[-3] == '-')
                    token->rstrip = tokenizer->lstrip_flag = 1;
                goto found;
            }
            // unterminated variable
            cursor = tokenizer->cursor + 2;
            tokenizer->lstrip_flag = 0;
            goto found;
        }
    }
    cursor = last + 1;
    token->lstrip = tokenizer->lstrip_flag;
    tokenizer->lstrip_flag = 0;
found:
    token->length = cursor - tokenizer->cursor;
    tokenizer->cursor += token->length;
    tokenizer->length -= token->length;

    if (tokenizer->line_number) {
        const char *cursor = token->str;
        const char *end = token->str + token->length;
        while (cursor < end) {
            if (*cursor == '\n')
                tokenizer->line_number++;
            cursor++;
        }
    }
}

static VALUE tokenizer_shift_method(VALUE self)
{
    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(self, tokenizer);

    token_t token;
    tokenizer_next(tokenizer, &token);
    if (!token.type)
        return Qnil;

    return rb_enc_str_new(token.str, token.length, utf8_encoding);
}

static VALUE tokenizer_line_number_method(VALUE self)
{
    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(self, tokenizer);

    if (tokenizer->line_number == 0)
        return Qnil;

    return UINT2NUM(tokenizer->line_number);
}

void init_liquid_tokenizer()
{
    cLiquidTokenizer = rb_define_class_under(mLiquidC, "Tokenizer", rb_cObject);
    rb_define_alloc_func(cLiquidTokenizer, tokenizer_allocate);
    rb_define_method(cLiquidTokenizer, "initialize", tokenizer_initialize_method, 2);
    rb_define_method(cLiquidTokenizer, "shift", tokenizer_shift_method, 0);
    rb_define_method(cLiquidTokenizer, "line_number", tokenizer_line_number_method, 0);
}

