#include "liquid.h"
#include "lexer.h"
#include <stdio.h>

const char *symbol_names[TOKEN_END] = {
    [LEXER_TOKEN_NONE] = "none",
    [TOKEN_COMPARISON] = "comparison",
    [TOKEN_QUOTE] = "string",
    [TOKEN_NUMBER] = "number",
    [TOKEN_IDENTIFIER] = "id",
    [TOKEN_DOTDOT] = "dotdot",
    [TOKEN_EOS] = "end_of_string",
    [TOKEN_PIPE] = "pipe",
    [TOKEN_DOT] = "dot",
    [TOKEN_COLON] = "colon",
    [TOKEN_COMMA] = "comma",
    [TOKEN_OPEN_SQUARE] = "open_square",
    [TOKEN_CLOSE_SQUARE] = "close_square",
    [TOKEN_OPEN_ROUND] = "open_round",
    [TOKEN_CLOSE_ROUND] = "close_round"
};

static VALUE cLiquidLexer;
static VALUE symbols[TOKEN_END] = {0};

static VALUE get_rb_type(unsigned char type)
{
    VALUE ret = symbols[type];
    if (ret) return ret;

    ret = ID2SYM(rb_intern(symbol_names[type]));
    symbols[type] = ret;
    return ret;
}

inline static int is_identifier(char c)
{
    return ISALNUM(c) || c == '_' || c == '-' || c == '?' || c == '!';
}

inline static int is_special(char c)
{
    switch (c) {
        case '|': case '.': case ':': case ',':
        case '[': case ']': case '(': case ')':
            return 1;
    }
    return 0;
}

inline static const char *skip_white(const char *cur, const char *end)
{
    while (cur < end && ISSPACE(*cur)) ++cur;
    return cur;
}

// Returns a pointer to the character after the end of the match.
inline static const char *prefix_end(const char *cur, const char *end, const char *pattern)
{
    size_t pattern_len = strlen(pattern);

    if (pattern_len > (size_t)(end - cur)) return NULL;
    if (memcmp(cur, pattern, pattern_len) != 0) return NULL;

    return cur + pattern_len;
}

inline static const char *scan_past(const char *cur, const char *end, char target)
{
    const char *match = memchr(cur + 1, target, end - cur - 1);
    return match ? match + 1 : NULL;
}

inline static int is_escaped(const char *start, const char *cur)
{
    char escaped = 0;

    while (--cur >= start && *cur == '\\')
        escaped = !escaped;

    return escaped;
}

#define RETURN_TOKEN(t, n) { \
                               token->type = (t); \
                               token->val = start; \
                               return (token->val_end = start + (n)); \
                           }

// Reads one token from str, and fills it into the token argument.
// Returns the start of the next token if any, otherwise the end of the string.
const char *lex_one(const char *str, const char *end, lexer_token_t *token)
{
    str = skip_white(str, end);
    if (str >= end) return str;

    const char *start = str;
    char c = *str, cn = 0;
    if (str + 1 < end) cn = str[1];

    switch (c) {
        case '<':
            RETURN_TOKEN(TOKEN_COMPARISON, cn == '>' || cn == '=' ? 2 : 1);
        case '>':
            RETURN_TOKEN(TOKEN_COMPARISON, cn == '=' ? 2 : 1);
        case '=':
        case '!':
            if (cn == '=') RETURN_TOKEN(TOKEN_COMPARISON, 2);
            break;
        case '.':
            if (cn == '.') RETURN_TOKEN(TOKEN_DOTDOT, 2);
            break;
    }

    if ((str = prefix_end(start, end, "contains")))
        RETURN_TOKEN(TOKEN_COMPARISON, str - start);

    if (c == '\'' || c == '"') {
        str = start;
        do {
            str = scan_past(str, end, c);
        } while (str && is_escaped(start, str));

        if (str) {
            // Quote was properly terminated.
            RETURN_TOKEN(TOKEN_QUOTE, str - start);
        }
    }

    if (ISDIGIT(c) || c == '-') {
        int has_dot = 0;
        str = start;
        while (++str < end) {
            if (!has_dot && *str == '.') {
                has_dot = 1;
            } else if (!ISDIGIT(*str)) {
                break;
            }
        }

        if (*str == '.')
            str--; // Ignore any trailing dot.

        if (str[-1] != '-')
            RETURN_TOKEN(TOKEN_NUMBER, str - start);
    }

    if (is_identifier(c) && c != '-') {
        str = start;
        while (++str < end && is_identifier(*str)) {}
        RETURN_TOKEN(TOKEN_IDENTIFIER, str - start);
    }

    if (is_special(c)) RETURN_TOKEN(c, 1);

    rb_raise(cLiquidSyntaxError, "Unexpected character %c", c);
    return NULL;
}

#undef RETURN_TOKEN

// Lexes the entire input string into a Ruby array.
// Should only be called from Ruby, otherwise use lex_one.
VALUE rb_lex(VALUE self, VALUE markup)
{
    StringValue(markup);

    const char *str = RSTRING_PTR(markup);
    const char *end = str + RSTRING_LEN(markup);

    lexer_token_t token;
    VALUE output = rb_ary_new();

    while (str < end) {
        token.type = 0;
        str = lex_one(str, end, &token);

        if (token.type) {
            VALUE rb_token = rb_ary_new3(2, get_rb_type(token.type), TOKEN_STR(token));
            rb_ary_push(output, rb_token);
        }
    }

    return rb_ary_push(output, rb_ary_new3(1, get_rb_type(TOKEN_EOS)));
}

void init_liquid_lexer(void)
{
    cLiquidLexer = rb_const_get(mLiquid, rb_intern("Lexer"));
    rb_define_singleton_method(cLiquidLexer, "c_lex", rb_lex, 1);
}

