#include "liquid.h"
#include "tokenizer.h"
#include <assert.h>
#include <stdio.h>

static VALUE cLiquidVariable, cLiquidTemplate;
static ID idNodeList, idClear, idBlank, idBlankQ, idOptions, idTags, idUnknownTag,
          idAssertMissingDelim, idBlockDelimiter, idParse, idSquareBrackets;

int is_id(int c) {
    return rb_isalnum(c) || c == '_';
}

inline static const char *read_until(const char *start, const char *end, int (func)(int)) {
    while (start < end && func((unsigned char) *start)) start++;
    return start;
}

VALUE rb_block_parse(VALUE self, VALUE tokens)
{
    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(tokens, tokenizer);

    rb_funcall(Qnil, rb_intern("p"), 1, tokenizer->source);
    rb_ivar_set(self, idBlank, Qtrue);

    token_t token;
    VALUE tags = Qnil, options = rb_ivar_get(self, idOptions);
    VALUE block_delimiter = rb_funcall(self, idBlockDelimiter, 0);

    if (rb_ivar_get(self, idNodeList) == Qnil) {
        rb_ivar_set(self, idNodeList, rb_ary_new());
    } else {
        rb_funcall(rb_ivar_get(self, idNodeList), idClear, 0);
    }

    while(tokenizer_next(tokenizer, &token)) {
        rb_funcall(Qnil, rb_intern("p"), 1, rb_enc_str_new(token.str, token.length, utf8_encoding));
        if (token.type == TOKEN_RAW) {
            VALUE str = rb_enc_str_new(token.str, token.length, utf8_encoding);
            rb_ary_push(rb_ivar_get(self, idNodeList), str);

            if (rb_ivar_get(self, idBlank) == Qtrue) {
                const char *end = token.str + token.length;

                if (read_until(token.str, end, rb_isspace) < end)
                    rb_ivar_set(self, idBlank, Qfalse);
            }

        } else if (token.type == TOKEN_VARIABLE) {
            VALUE args[2] = {rb_enc_str_new(token.str + 2, token.length - 4, utf8_encoding), options};
            VALUE var = rb_class_new_instance(2, args, cLiquidVariable);
            rb_ary_push(rb_ivar_get(self, idNodeList), var);
            rb_ivar_set(self, idBlank, Qfalse);

        } else if (token.type == TOKEN_TAG) {
            const char *start = token.str + 2, *end = token.str + token.length - 2;
            assert(start <= end);

            const char *name_start = read_until(start, end, rb_isspace);
            const char *name_end = read_until(name_start, end, is_id);

            VALUE name = rb_enc_str_new(name_start, name_end - name_start, utf8_encoding);
            rb_funcall(Qnil, rb_intern("p"), 1, name);

            if (rb_equal(name, block_delimiter))
                return Qnil;

            if (tags == Qnil)
                tags = rb_funcall(cLiquidTemplate, idTags, 0);

            VALUE tag = rb_funcall(tags, idSquareBrackets, 1, name);
            rb_funcall(Qnil, rb_intern("p"), 1, tag);

            const char *markup_start = read_until(name_end, end, rb_isspace);
            VALUE markup = rb_enc_str_new(markup_start, end - markup_start, utf8_encoding);
            rb_funcall(Qnil, rb_intern("p"), 1, markup);

            if (tag != Qnil) {
                VALUE new_tag = rb_funcall(tag, idParse, 4, name, markup, tokens, options);

                if (rb_ivar_get(self, idBlank) == Qtrue)
                    rb_ivar_set(self, idBlank, rb_funcall(new_tag, idBlankQ, 0));

                rb_ary_push(rb_ivar_get(self, idNodeList), new_tag);
            } else {
                rb_funcall(self, idUnknownTag, 3, name, markup, tokens);
            }
        }
    }

    rb_funcall(self, idAssertMissingDelim, 0);
    return Qnil;
}

void init_liquid_block()
{
    idBlank = rb_intern("@blank");
    idNodeList = rb_intern("@nodelist");
    idOptions = rb_intern("@options");

    idBlankQ = rb_intern("blank?");
    idClear = rb_intern("clear");
    idTags = rb_intern("tags");
    idParse = rb_intern("parse");
    idUnknownTag = rb_intern("unknown_tag");
    idBlockDelimiter = rb_intern("block_delimiter");
    idAssertMissingDelim = rb_intern("assert_missing_delimitation!");
    idSquareBrackets = rb_intern("[]");

    VALUE cLiquidBlock = rb_const_get(mLiquid, rb_intern("Block"));
    cLiquidVariable = rb_const_get(mLiquid, rb_intern("Variable"));
    cLiquidTemplate = rb_const_get(mLiquid, rb_intern("Template"));
    rb_define_method(cLiquidBlock, "c_parse", rb_block_parse, 1);
}

