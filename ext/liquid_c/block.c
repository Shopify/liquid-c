#include "liquid.h"
#include "tokenizer.h"
#include "stringutil.h"
#include <stdio.h>

static ID
    intern_raise_missing_variable_terminator,
    intern_raise_missing_tag_terminator,
    intern_nodelist,
    intern_blank,
    intern_is_blank,
    intern_clear,
    intern_registered_tags,
    intern_parse,
    intern_square_brackets,
    intern_set_line_number,
    intern_unknown_tag_in_liquid_tag;

static VALUE cLiquidBlockBody;

static int is_id(int c)
{
    return rb_isalnum(c) || c == '_';
}

static VALUE yield_end_tag(VALUE tag_name, VALUE tag_markup, bool for_liquid_tag)
{
    if (!for_liquid_tag) {
        return rb_yield_values(2, tag_name, tag_markup);
    }

    VALUE args[] = { tag_name, tag_markup };
    return rb_funcall_passing_block(cLiquidBlockBody, intern_unknown_tag_in_liquid_tag, 2, args);
}

static VALUE internal_block_parse(VALUE self, VALUE tokens, VALUE parse_context, bool for_liquid_tag)
{
    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(tokens, tokenizer);

    token_t token;
    VALUE tags = Qnil;
    VALUE nodelist = rb_ivar_get(self, intern_nodelist);

    while (true) {
        int token_start_line_number = tokenizer->line_number;
        if (token_start_line_number != 0) {
            rb_funcall(parse_context, intern_set_line_number, 1, UINT2NUM(token_start_line_number));
        }
        tokenizer_next(tokenizer, &token);

        switch (token.type) {
            case TOKENIZER_TOKEN_NONE:
                if (!for_liquid_tag) {
                    return rb_yield_values(2, Qnil, Qnil);
                }
                return Qnil;

            case TOKEN_INVALID:
            {
                VALUE str = rb_enc_str_new(token.str_full, token.len_full, utf8_encoding);

                ID raise_method_id = intern_raise_missing_variable_terminator;
                if (token.str_full[1] == '%') raise_method_id = intern_raise_missing_tag_terminator;

                return rb_funcall(self, raise_method_id, 2, str, parse_context);
            }
            case TOKEN_RAW:
            {
                const char *start = token.str_full, *end = token.str_full + token.len_full;
                const char *token_start = start, *token_end = end;

                if (token.lstrip)
                    token_start = read_while(start, end, rb_isspace);

                if (token.rstrip)
                    token_end = read_while_reverse(token_start, end, rb_isspace);

                // Skip token entirely if there is no data to be rendered.
                if (token_start == token_end)
                    break;

                VALUE str = rb_enc_str_new(token_start, token_end - token_start, utf8_encoding);
                rb_ary_push(nodelist, str);

                if (rb_ivar_get(self, intern_blank) == Qtrue) {
                    const char *end = token.str_full + token.len_full;

                    if (read_while(token.str_full, end, rb_isspace) < end)
                        rb_ivar_set(self, intern_blank, Qfalse);
                }
                break;
            }
            case TOKEN_VARIABLE:
            {
                VALUE args[2] = {rb_enc_str_new(token.str_trimmed, token.len_trimmed, utf8_encoding), parse_context};
                VALUE var = rb_class_new_instance(2, args, cLiquidVariable);
                rb_ary_push(nodelist, var);
                rb_ivar_set(self, intern_blank, Qfalse);
                break;
            }
            case TOKEN_TAG:
            {
                const char *start = token.str_trimmed, *end = token.str_trimmed + token.len_trimmed;

                // Imitate \s*(\w+)\s*(.*)? regex
                const char *name_start = read_while(start, end, rb_isspace);
                const char *name_end = read_while(name_start, end, is_id);
                long name_len = name_end - name_start;

                if (name_len == 0) {
                    VALUE str = rb_enc_str_new(token.str_trimmed, token.len_trimmed, utf8_encoding);
                    return yield_end_tag(str, str, for_liquid_tag);
                }

                if (name_len == 6 && strncmp(name_start, "liquid", 6) == 0) {
                    const char *markup_start = read_while(name_end, end, rb_isspace);
                    int line_number = token_start_line_number;
                    if (line_number) {
                        line_number += count_newlines(token.str_full, markup_start);
                    }
                    VALUE liquid_tag_tokenizer = tokenizer_new_from_cstr(
                        tokenizer->source,
                        markup_start,
                        end,
                        line_number,
                        true
                    );
                    internal_block_parse(self, liquid_tag_tokenizer, parse_context, true);
                    break;
                }

                VALUE tag_name = rb_enc_str_new(name_start, name_end - name_start, utf8_encoding);

                if (tags == Qnil)
                    tags = rb_funcall(self, intern_registered_tags, 0);

                VALUE tag_class = rb_funcall(tags, intern_square_brackets, 1, tag_name);

                const char *markup_start = read_while(name_end, end, rb_isspace);
                VALUE markup = rb_enc_str_new(markup_start, end - markup_start, utf8_encoding);

                if (tag_class == Qnil)
                    return yield_end_tag(tag_name, markup, for_liquid_tag);

                VALUE new_tag = rb_funcall(tag_class, intern_parse, 4, tag_name, markup, tokens, parse_context);

                if (rb_ivar_get(self, intern_blank) == Qtrue && !RTEST(rb_funcall(new_tag, intern_is_blank, 0)))
                    rb_ivar_set(self, intern_blank, Qfalse);

                rb_ary_push(nodelist, new_tag);
                break;
            }
        }
    }
    return Qnil;
}

static VALUE rb_block_parse(VALUE self, VALUE tokens, VALUE parse_context)
{
    return internal_block_parse(self, tokens, parse_context, false);
}

void init_liquid_block()
{
    intern_raise_missing_variable_terminator = rb_intern("raise_missing_variable_terminator");
    intern_raise_missing_tag_terminator = rb_intern("raise_missing_tag_terminator");
    intern_nodelist = rb_intern("@nodelist");
    intern_blank = rb_intern("@blank");
    intern_is_blank = rb_intern("blank?");
    intern_clear = rb_intern("clear");
    intern_registered_tags = rb_intern("registered_tags");
    intern_parse = rb_intern("parse");
    intern_square_brackets = rb_intern("[]");
    intern_set_line_number = rb_intern("line_number=");
    intern_unknown_tag_in_liquid_tag = rb_intern("unknown_tag_in_liquid_tag");

    cLiquidBlockBody = rb_const_get(mLiquid, rb_intern("BlockBody"));
    rb_global_variable(&cLiquidBlockBody);
    
    rb_define_method(cLiquidBlockBody, "c_parse", rb_block_parse, 2);
}

