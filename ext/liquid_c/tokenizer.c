#include "liquid.h"
#include "tokenizer.h"
#include "stringutil.h"

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

static VALUE tokenizer_initialize_method(VALUE self, VALUE source, VALUE start_line_number, VALUE for_liquid_tag)
{
    tokenizer_t *tokenizer;

    Check_Type(source, T_STRING);
    Tokenizer_Get_Struct(self, tokenizer);
    source = rb_str_dup_frozen(source);
    tokenizer->source = source;
    tokenizer->cursor = RSTRING_PTR(source);
    tokenizer->cursor_end = tokenizer->cursor + RSTRING_LEN(source);
    tokenizer->lstrip_flag = false;
    // tokenizer->line_number keeps track of the current line number or it is 0
    // to indicate that line numbers aren't being calculated
    tokenizer->line_number = FIX2UINT(start_line_number);
    tokenizer->for_liquid_tag = RTEST(for_liquid_tag);
    return Qnil;
}

// Internal method for creating a tokenizer from C.
// This does not create a copy of the source string, and should be called with
// a `source` value taken from an existing tokenizer.
VALUE tokenizer_new_from_cstr(VALUE source, const char *cursor, const char *cursor_end, int line_number, bool for_liquid_tag)
{
    Check_Type(source, T_STRING);

    VALUE rbtokenizer = tokenizer_allocate(cLiquidTokenizer);

    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(rbtokenizer, tokenizer);

    tokenizer->source = source;
    tokenizer->cursor = cursor;
    tokenizer->cursor_end = cursor_end;
    tokenizer->lstrip_flag = false;
    tokenizer->line_number = line_number;
    tokenizer->for_liquid_tag = for_liquid_tag;
    return rbtokenizer;
}

// Tokenizes contents of {% liquid ... %}
static void tokenizer_next_for_liquid_tag(tokenizer_t *tokenizer, token_t *token)
{
    const char *end = tokenizer->cursor_end;

    token->str_full = tokenizer->cursor;
    token->str_trimmed = read_while(token->str_full, end, rb_isspace);

    const char *start_next_line = read_while(token->str_trimmed, end, not_newline);
    const char *end_trimmed = read_while_reverse(token->str_trimmed, start_next_line, rb_isspace);
    token->len_trimmed = end_trimmed - token->str_trimmed;

    const char *end_full = read_while(start_next_line, end, rb_isspace);
    token->len_full = end_full - token->str_full;

    if (token->len_trimmed == 0) {
        // reached end of tag without finding a token
        token->type = TOKENIZER_TOKEN_NONE;
    } else {
        token->type = TOKEN_TAG;
    }

    tokenizer->cursor = end_full;

    if (tokenizer->line_number) {
        tokenizer->line_number += count_newlines(token->str_full, end_full);
    }
}

// Tokenizes contents of a full Liquid template
static void tokenizer_next_for_template(tokenizer_t *tokenizer, token_t *token)
{
    const char *cursor = tokenizer->cursor;
    const char *last = tokenizer->cursor_end - 1;

    token->str_full = cursor;
    token->type = TOKEN_RAW;

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
            tokenizer->lstrip_flag = false;
            goto found;
        }
        tokenizer->lstrip_flag = false;
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
                    token->rstrip = tokenizer->lstrip_flag = true;
                goto found;
            }
            // unterminated tag
            cursor = tokenizer->cursor + 2;
            tokenizer->lstrip_flag = false;
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
                    token->rstrip = tokenizer->lstrip_flag = true;
                goto found;
            }
            // unterminated variable
            cursor = tokenizer->cursor + 2;
            tokenizer->lstrip_flag = false;
            goto found;
        }
    }
    cursor = last + 1;
    token->lstrip = tokenizer->lstrip_flag;
    tokenizer->lstrip_flag = false;
found:
    token->len_full = cursor - token->str_full;

    token->str_trimmed = token->str_full;
    token->len_trimmed = token->len_full;

    if (token->type == TOKEN_VARIABLE || token->type == TOKEN_TAG) {
        token->str_trimmed += 2 + token->lstrip;
        token->len_trimmed -= 2 + token->lstrip + 2 + token->rstrip;
    }

    tokenizer->cursor += token->len_full;

    if (tokenizer->line_number) {
        tokenizer->line_number += count_newlines(token->str_full, token->str_full + token->len_full);
    }
}

void tokenizer_next(tokenizer_t *tokenizer, token_t *token)
{
    memset(token, 0, sizeof(*token));

    if (tokenizer->cursor >= tokenizer->cursor_end) {
        return;
    }
    if (tokenizer->for_liquid_tag) {
        tokenizer_next_for_liquid_tag(tokenizer, token);
    } else {
        tokenizer_next_for_template(tokenizer, token);
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

    // When sent back to Ruby, tokens are the raw string including whitespace
    // and tag delimiters. It should be possible to reconstruct the exact
    // template from the tokens.
    return rb_enc_str_new(token.str_full, token.len_full, utf8_encoding);
}

static VALUE tokenizer_shift_trimmed_method(VALUE self)
{
    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(self, tokenizer);

    token_t token;
    tokenizer_next(tokenizer, &token);
    if (!token.type)
        return Qnil;

    // This method doesn't include whitespace and tag delimiters. It allows for
    // testing the output of tokenizer_next as used by rb_block_parse.
    return rb_enc_str_new(token.str_trimmed, token.len_trimmed, utf8_encoding);
}

static VALUE tokenizer_line_number_method(VALUE self)
{
    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(self, tokenizer);

    if (tokenizer->line_number == 0)
        return Qnil;

    return UINT2NUM(tokenizer->line_number);
}

static VALUE tokenizer_for_liquid_tag_method(VALUE self)
{
    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(self, tokenizer);

    return tokenizer->for_liquid_tag ? Qtrue : Qfalse;
}

void init_liquid_tokenizer()
{
    cLiquidTokenizer = rb_define_class_under(mLiquidC, "Tokenizer", rb_cObject);
    rb_define_alloc_func(cLiquidTokenizer, tokenizer_allocate);
    rb_define_method(cLiquidTokenizer, "initialize", tokenizer_initialize_method, 3);
    rb_define_method(cLiquidTokenizer, "shift", tokenizer_shift_method, 0);
    rb_define_method(cLiquidTokenizer, "line_number", tokenizer_line_number_method, 0);
    rb_define_method(cLiquidTokenizer, "for_liquid_tag", tokenizer_for_liquid_tag_method, 0);

    // For testing the internal token representation.
    rb_define_private_method(cLiquidTokenizer, "shift_trimmed", tokenizer_shift_trimmed_method, 0);
}

