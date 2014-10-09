
#include "liquid.h"
#include "variable.h"
#include <stdio.h>

/*
 * Character type lookup table
 * 1: Whitespace (Characters that would be matched by \s in regexp's)
 * 2: Word characters (Characters under \w)
 * 3: Characters that, along with group 1, would terminate a quoted fragment
 * 4: Quote delimiters
 */
static const unsigned char char_lookup[256] = {
    [' '] = 1, ['\n'] = 1, ['\r'] = 1, ['\t'] = 1, ['\f'] = 1,
    ['a'] = 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    ['A'] = 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    ['0'] = 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    ['_'] = 2,
    [','] = 3, ['|'] = 3,
    ['\''] = 4,
    ['\"'] = 4,
};

inline static unsigned char is_white(char c) {
    return char_lookup[(unsigned char)c] == 1;
}

inline static unsigned char is_word_char(char c) {
    return char_lookup[(unsigned char)c] == 2;
}

inline static unsigned char is_quote_delimiter(char c) {
    return char_lookup[(unsigned char)c] == 4;
}

inline static unsigned char is_quoted_fragment_terminator(char c) {
    return char_lookup[(unsigned char)c] & 1;
}

inline static const char *scan_past(const char *cur, const char *end, char target) {
    while (cur < end) {
        if (*cur++ == target) {
            return cur;
        }
    }
    return NULL;
}

inline static const char *skip_white(const char *cur, const char *end) {
    while (cur < end && is_white(*cur)) ++cur;
    return cur;
}

/*
 * A "quoted fragment" is either a quoted string, e.g., 'the "quick" brown fox'
 * or a sequence of characters that ends in whitespace, ',', or '|'. However,
 * this sequence can contain in it quoted strings in which whitespace, ',', or
 * '|' can appear.
 */
const char *scan_for_quoted_fragment(const char **cur_ptr, const char *end)
{
    const char *cur = *cur_ptr;
    const char *fragment_start;

    while (cur < end) {
        char c = *cur;
        if (is_quoted_fragment_terminator(c)) {
            cur++;
            continue;
        } else if (is_quote_delimiter(c)) {
            fragment_start = cur++;
            const char *fragment_end = scan_past(cur, end, c);
            if (fragment_end) {
                *cur_ptr = fragment_end;
                return fragment_start;
            }
            cur++;
            continue;
        }
        break;
    }
    if (cur >= end) {
        return NULL;
    }
    fragment_start = cur;

    while (cur < end) {
        char c = *cur;
        if (is_quoted_fragment_terminator(c)) {
            break;
        } else if (is_quote_delimiter(c)) {
            const char *quote_end = scan_past(cur + 1, end, c);
            if (!quote_end) {
                break;
            }
            cur = quote_end;
        } else {
            cur++;
        }
    }
    *cur_ptr = cur;

    return fragment_start;
}

const char *parse_bare_fragment(const char *cur, const char *end)
{
    while (cur < end && !is_quoted_fragment_terminator(*cur)) {
        char c = *cur++;
        if (is_quote_delimiter(c)) {
            const char *quote_end = scan_past(cur, end, c);
            if (!quote_end) {
                return cur;
            }
            cur = quote_end;
        }
    }
    return cur;
}

const char *parse_quoted_fragment(const char *cur, const char *end)
{
    if (cur >= end) return NULL;
    char c = *cur;
    if (is_quote_delimiter(c)) {
        return scan_past(cur + 1, end, c);
    }
    return parse_bare_fragment(cur, end);
}

static const char *parse_filter_arg(VALUE args, const char *cur, const char *end)
{
    /*
     * Arguments are separated by either ':' or ','
     * Arguments can sometimes contain one ':' e.g.
     * "hello" | translate lang : latin : upsidedown : yes
     *                          ^a      ^b           ^c
     * ^a and ^c are part of the arguments themselves, while ^b is a separator.
     * The arguments would then be:
     *      * lang : latin
     *      * upsidedown : yes
     */
    cur = skip_white(cur, end);
    if (cur >= end) return cur;

    const char *arg_begin = cur;

    /* optional keyword argument */
    if (is_word_char(*cur)) {
        cur++;
        while (is_word_char(*cur)) cur++;

        const char *word_chars_end = cur;

        cur = skip_white(cur, end);

        if (cur + 1 < end && *cur == ':') {
            cur++;
            cur = skip_white(cur + 1, end);

            cur = parse_quoted_fragment(cur, end);
            if (cur) {
                rb_ary_push(args, rb_enc_str_new(arg_begin, cur - arg_begin, utf8_encoding));
                return cur;
            }
        }
        /* not a keyword argument, so parse the rest of the positional argument */
        cur = parse_bare_fragment(word_chars_end - 1, end);
        rb_ary_push(args, rb_enc_str_new(arg_begin, cur - arg_begin, utf8_encoding));
        return cur;
    }
    /* positional argument */
    cur = parse_quoted_fragment(cur, end);
    if (!cur) return arg_begin;

    rb_ary_push(args, rb_enc_str_new(arg_begin, cur - arg_begin, utf8_encoding));
    return cur;
}

static const char *scan_filter_arg(VALUE args, const char *cur, const char *filter_end)
{
    while (cur < filter_end) {
        char c = *cur++;
        if (c == ':' || c == ',') {
            return parse_filter_arg(args, cur, filter_end);
        }
    }
    return cur;
}

static const char *parse_filter(VALUE filters, const char *cur, const char *end)
{
    /*
     * Filters are separated by any number of |s
     * Filters can have either a : or , before their arguments list
     */

    const char *start = cur;

    // Find end of filter
    while (cur < end) {
        char c = *cur;
        if (c == '|') {
            break;
        } else if (is_quote_delimiter(c)) {
            const char *quote_end = scan_past(cur + 1, end, c);
            if (!quote_end) {
                break;
            }
            cur = quote_end;
        } else {
            cur++;
        }
    }
    const char *filter_end = cur;
    cur = start;

    // Parse filter name
    while (cur < filter_end && !is_word_char(*cur)) ++cur;
    if (cur >= filter_end) return filter_end;
    const char *filter_name_start = cur;
    while (cur < filter_end && is_word_char(*cur)) ++cur;
    size_t filter_name_len = cur - filter_name_start;
    VALUE filter_name = rb_enc_str_new((const char *)filter_name_start, filter_name_len, utf8_encoding);

    // Parse filter arguments
    cur = start;
    VALUE args = rb_ary_new();
    rb_ary_push(filters, rb_ary_new3(2, filter_name, args));
    while (cur < filter_end) {
        cur = scan_filter_arg(args, cur, filter_end);
    }

    return filter_end;
}

static VALUE lax_parse(VALUE self, VALUE markup)
{
    const char *markup_ptr = RSTRING_PTR(markup);
    size_t markup_len = RSTRING_LEN(markup);

    VALUE filters = rb_ary_new();
    rb_iv_set(self, "@filters", filters);

    const char *end = markup_ptr + markup_len;
    const char *cur = markup_ptr;

    // Parse name
    const char *name_begin = scan_for_quoted_fragment(&cur, end);
    if (!name_begin) return Qnil;
    size_t name_len = cur - name_begin;
    rb_iv_set(self, "@name", rb_enc_str_new(name_begin, name_len, utf8_encoding));

    // Parse filters
    cur = scan_past(cur, end, '|');
    if (!cur) return Qnil;
    while (cur < end) {
        cur = skip_white(cur, end);
        cur = parse_filter(filters, cur, end);
        cur++; /* skip filter separator */
    }

    return Qnil;
}

void init_liquid_variable(void)
{
    VALUE cLiquidVariable = rb_define_class_under(mLiquid, "Variable", rb_cObject);
    rb_define_method(cLiquidVariable, "lax_parse", lax_parse, 1);
}
