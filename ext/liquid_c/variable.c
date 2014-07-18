
#include "liquid.h"
#include "variable.h"
#include "stdio.h"

static VALUE cLiquidVariableParser;

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
    variable_parser->filters = rb_ary_new();
    return obj;
}

static const unsigned char char_lookup[256] = {
    [' '] = 1, ['\n'] = 1, ['\r'] = 1, ['\t'] = 1, ['\f'] = 1,
    ['a'] = 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    ['A'] = 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    ['_'] = 2,
    [','] = 3, ['|'] = 3,
    ['\''] = 4,
    ['\"'] = 4,
};

inline const char *variable_parser_parse_quoted_fragment(const char *cur, const char *end)
{
    if (cur >= end) return NULL;
    if (*cur == '\'') {
        ++cur;
        while (cur < end && *cur != '\'') ++cur;
        if (cur == end) return NULL;
        ++cur;
        return cur;
    } else if (*cur == '\"') {
        ++cur;
        while (cur < end && *cur != '\"') ++cur;
        if (cur == end) return NULL;
        ++cur;
        return cur;
    }
    while (cur < end && !(char_lookup[(unsigned char)*cur] & 1)) {
        if (*cur == '\'') {
            ++cur;
            while (cur < end && *cur != '\'') ++cur;
            if (cur == end) return NULL;
            ++cur;
        } else if (*cur == '\"') {
            ++cur;
            while (cur < end && *cur != '\"') ++cur;
            if (cur == end) return NULL;
            ++cur;
        } else {
            ++cur;
        }
    }
    return cur;
}

void variable_parser_parse(variable_parser_t *parser, const char* markup, size_t markup_len)
{
#define SKIP_WHITE do { while (cur < end && char_lookup[(unsigned char)*cur] == 1) ++cur; } while (0)
    const char *end = markup + markup_len;
    const char *cur = markup;
    SKIP_WHITE;
    // Parse name
    const char *name_begin = cur;
    cur = variable_parser_parse_quoted_fragment(cur, end);
    if (!cur) return;
    size_t name_len = cur - name_begin;
    parser->name = rb_enc_str_new(name_begin, name_len, utf8_encoding);
    SKIP_WHITE;
    // Parse filters
    while (cur < end) {
        while (cur < end && *cur == '|') {
            ++cur;
            SKIP_WHITE;
        }
        if (cur >= end) return;
        const char *filter_name_start = cur;
        while (cur < end && char_lookup[(unsigned char)*cur] == 2) ++cur;
        size_t filter_name_len = cur - filter_name_start;
        VALUE args = rb_ary_new();
        rb_ary_push(parser->filters, rb_ary_new3(2,
                    rb_enc_str_new(filter_name_start, filter_name_len, utf8_encoding), args));
        SKIP_WHITE;
        while (cur < end) {
            if (*cur == '|') break;
            if (*cur == ':' || *cur == ',') {
                ++cur;
                SKIP_WHITE;
            }
            const char *arg_begin = cur;
            cur = variable_parser_parse_quoted_fragment(cur, end);
            size_t arg_len = cur - arg_begin;
            if (!cur) return;
            if (char_lookup[(unsigned char)*arg_begin] == 4) {
                rb_ary_push(args, rb_enc_str_new(arg_begin, arg_len, utf8_encoding));
                SKIP_WHITE;
                continue;
            }
            SKIP_WHITE;
            if (cur < end && *cur == ':') {
                // arg_begin .. arg_len is actually the argument name
                ++cur;
                SKIP_WHITE;
                cur = variable_parser_parse_quoted_fragment(cur, end);
                if (!cur) return;
                arg_len = cur - arg_begin;
            }
            rb_ary_push(args, rb_enc_str_new(arg_begin, arg_len, utf8_encoding));
            SKIP_WHITE;
        }
    }
#undef SKIP_WHITE
}

static VALUE variable_parser_initialize_method(VALUE self, VALUE markup)
{
    variable_parser_t *parser;
    Variable_Parser_Get_Struct(self, parser);
    variable_parser_parse(parser, RSTRING_PTR(markup), RSTRING_LEN(markup));
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
