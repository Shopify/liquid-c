
#include "liquid.h"
#include "variable.h"
#include <stdio.h>

VALUE cLiquidVariableParser;

static void variable_parser_mark(void *ptr)
{
    variable_parser_t *parser = ptr;
    rb_gc_mark(parser->name);
    rb_gc_mark(parser->filters);
}

static void variable_parser_free(void *ptr)
{
    variable_parser_t *parser = ptr;
    xfree(parser);
}

static size_t variable_parser_memsize(const void *ptr)
{
    return ptr ? sizeof(variable_parser_t) : 0;
}

const rb_data_type_t variable_parser_data_type =
{
    "liquid_variable_parser",
    { variable_parser_mark, variable_parser_free, variable_parser_memsize, },
#if defined(RUBY_TYPED_FREE_IMMEDIATELY)
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
#endif
};

static VALUE variable_parser_allocate(VALUE klass)
{
    VALUE obj;
    variable_parser_t *variable_parser;
    obj = TypedData_Make_Struct(klass, variable_parser_t, &variable_parser_data_type, variable_parser);
    variable_parser->name = Qnil;
    variable_parser->filters = rb_ary_new();
    return obj;
}

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

inline static unsigned char is_quoted_fragment_terminator(char c) {
    return char_lookup[(unsigned)c] & 1;
}

inline static unsigned char is_word_char(char c) {
    return char_lookup[(unsigned)c] == 2;
}

inline static unsigned char is_quote_delimiter(char c) {
    return char_lookup[(unsigned)c] == 4;
}

inline static const char *scan_past(const char *cur, const char *end, char target) {
    ++cur;
    while (cur < end && *cur != target) ++cur;
    if (cur == end) return NULL;
    ++cur;
    return cur;
}

#define TRY_PARSE(x) do { if ((cur = (x)) == NULL) return NULL; } while (0)

inline static const char *skip_white(const char *cur, const char *end) {
    while (cur < end && char_lookup[(unsigned)*cur] == 1) ++cur;
    return cur;
}

/*
 * A "quoted fragment" is either a quoted string, e.g., 'the "quick" brown fox'
 * or a sequence of characters that ends in whitespace, ',', or '|'. However,
 * this sequence can contain in it quoted strings in which whitespace, ',', or
 * '|' can appear.
 */
const char *parse_quoted_fragment(const char *cur, const char *end)
{
    if (cur >= end) {
        return NULL;
    }
    char start = *cur;
    if (is_quote_delimiter(start)) {
        return scan_past(cur, end, start);
    }
    while (cur < end && !is_quoted_fragment_terminator(*cur)) {
        start = *cur;
        if (is_quote_delimiter(start)) {
            TRY_PARSE(scan_past(cur, end, start));
        } else {
            ++cur;
        }
    }
    return cur;
}

static const char *parse_filter_item(VALUE args, const char *cur, const char *end)
{
    /*
     * Arguments are separated by ethier : or ,
     * Arguments can sometimes contain one :, e.g.:
     * "hello" | translate lang : latin : upsidedown : yes"
     *                          ^a      ^b           ^c
     * ^a and ^c are part of the arguments themselves, while ^b is a separator.
     * The arguments would then be:
     *      * lang : latin
     *      * upsidedown : yes
     */
    if (*cur == ':' || *cur == ',') {
        ++cur;
        cur = skip_white(cur, end);
    }
    const char *arg_begin = cur;
    TRY_PARSE(parse_quoted_fragment(cur, end));
    size_t arg_len = cur - arg_begin;
    if (!is_quote_delimiter(*arg_begin)) {
        cur = skip_white(cur, end);
        if (cur < end && *cur == ':') {
            ++cur;
            cur = skip_white(cur, end);
            TRY_PARSE(parse_quoted_fragment(cur, end));
            arg_len = cur - arg_begin;
        }
    }
    rb_ary_push(args, rb_enc_str_new(arg_begin, arg_len, utf8_encoding));
    cur = skip_white(cur, end);
    return cur;
}

static const char *parse_filter(variable_parser_t *parser, const char *cur, const char *end)
{
    /*
     * Filters are separated by any number of |s
     * Filters can have either a : or , before their arguments list
     */
    while (cur < end && *cur == '|') {
        ++cur;
        cur = skip_white(cur, end);
    }
    if (cur >= end) return NULL;
    // Parse filter name
    const char *filter_name_start = cur;
    while (cur < end && is_word_char(*cur)) ++cur;
    size_t filter_name_len = cur - filter_name_start;
    // Build arguments
    VALUE args = rb_ary_new();
    rb_ary_push(parser->filters, rb_ary_new3(2,
                rb_enc_str_new(filter_name_start, filter_name_len, utf8_encoding), args));
    cur = skip_white(cur, end);
    while (cur && cur < end) {
        if (*cur == '|') break;
        cur = parse_filter_item(args, cur, end);
    }
    return cur;
}

static void parse(variable_parser_t *parser, const char *markup, size_t markup_len)
{
    const char *end = markup + markup_len;
    const char *cur = markup;
    cur = skip_white(cur, end);
    // Parse name
    const char *name_begin = cur;
    cur = parse_quoted_fragment(cur, end);
    if (!cur) return;
    size_t name_len = cur - name_begin;
    parser->name = rb_enc_str_new(name_begin, name_len, utf8_encoding);
    cur = skip_white(cur, end);
    // Parse filters
    while (cur && cur < end) {
        cur = parse_filter(parser, cur, end);
    }
}

static VALUE variable_parser_initialize_method(VALUE self, VALUE markup)
{
    variable_parser_t *parser;
    Variable_Parser_Get_Struct(self, parser);
    parse(parser, RSTRING_PTR(markup), RSTRING_LEN(markup));
    rb_iv_set(self, "@name", parser->name);
    rb_iv_set(self, "@filters", parser->filters);
    return Qnil;
}

void init_liquid_variable(void)
{
    cLiquidVariableParser = rb_define_class_under(mLiquid, "VariableParse", rb_cObject);
    rb_define_alloc_func(cLiquidVariableParser, variable_parser_allocate);
    rb_define_method(cLiquidVariableParser, "initialize", variable_parser_initialize_method, 1);
    rb_define_attr(cLiquidVariableParser, "filters", 1, 0);
    rb_define_attr(cLiquidVariableParser, "name", 1, 0);
}
