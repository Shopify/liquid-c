#include "liquid.h"
#include "lexer.h"
#include <stdio.h>
#include <ctype.h>

const char *symbol_names[256] = {
    [0] = "nil",
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
static VALUE symbols[256] = {0};

static VALUE get_rb_type(unsigned char type) {
    VALUE ret = symbols[type];
    if (ret) return ret;

    ret = ID2SYM(rb_intern(symbol_names[type]));
    symbols[type] = ret;
    return ret;
}

static void raise_error(char unexpected) {
    rb_raise(cLiquidSyntaxError, "Unexpected character %c", unexpected);
}

inline static char is_identifier(char c) {
    return isalnum(c) || c == '_' || c == '-' || c == '?' || c == '!';
}

inline static char is_special(char c) {
    switch (c) {
        case '|': case '.': case ':': case ',':
        case '[': case ']': case '(': case ')':
            return 1;
    }
    return 0;
}

inline static const char *skip_white(const char *cur, const char *end) {
    while (cur < end && isspace(*cur)) ++cur;
    return cur;
}

// Returns a pointer to the character after the end of the match.
inline static const char *prefix_matches(const char *cur, const char *end, const char *pattern) {
    size_t pattern_len = strlen(pattern);

    if (pattern_len > (size_t)(end - cur)) return NULL;
    if (memcmp(cur, pattern, pattern_len) != 0) return NULL;

    return cur + pattern_len;
}

inline static const char *scan_past(const char *cur, const char *end, char target) {
    while (++cur < end && *cur != target);
    return cur >= end ? NULL : cur + 1;
}

inline static char is_escaped(const char *start, const char *cur) {
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

const char *lex_one(const char *str, const char *end, lexer_token_t *token) {
    str = skip_white(str, end);
    if (str >= end) return str;

    const char *start = str;
    char c = *str, cn = 0;

    if (++str < end) cn = *str;

    // Comparison.
    if (c == '<') {
        if (cn == '>' || cn == '=') {
            RETURN_TOKEN(TOKEN_COMPARISON, 2);
        } else {
            RETURN_TOKEN(TOKEN_COMPARISON, 1);
        }
    } else if (c == '>') {
        RETURN_TOKEN(TOKEN_COMPARISON, cn == '=' ? 2 : 1);
    } else if ((c == '=' || c == '!') && cn == '=') {
        RETURN_TOKEN(TOKEN_COMPARISON, 2);
    } else if ((str = prefix_matches(start, end, "contains"))) {
        RETURN_TOKEN(TOKEN_COMPARISON, str - start);
    }

    // Quotations.
    if (c == '\'' || c == '"') {
        str = start;
        while (true) {
            str = scan_past(str, end, c);
            if (!str || !is_escaped(start, str)) {
                break;
            }
        }

        if (str) RETURN_TOKEN(TOKEN_QUOTE, str - start);
    }

    // Numbers.
    if (isdigit(c) || c == '-') {
        char has_dot = 0;
        str = start;
        while (++str < end) {
            cn = *str;
            if (!has_dot && cn == '.') {
                has_dot = 1;
            } else if (!isdigit(cn)) {
                break;
            }
        }
        if (cn == '.') str--;
        RETURN_TOKEN(TOKEN_NUMBER, str - start);
    }

    // Identifiers.
    if (is_identifier(c)) {
        str = start;
        while (++str < end && is_identifier(*str)) {}
        RETURN_TOKEN(TOKEN_IDENTIFIER, str - start);
    }

    // Double dots.
    if (c == '.' && cn == '.') {
        RETURN_TOKEN(TOKEN_DOTDOT, 2);
    }

    // Specials.
    if (is_special(c)) {
        RETURN_TOKEN(c, 1);
    }

    raise_error(c);
    return NULL;
}

#undef RETURN_TOKEN

VALUE rb_lex(VALUE self, VALUE markup) {
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

    VALUE rb_eos = rb_ary_new();
    rb_ary_push(rb_eos, get_rb_type(TOKEN_EOS));
    rb_ary_push(output, rb_eos);
    return output;
}

void init_liquid_lexer(void)
{
    cLiquidLexer = rb_const_get(mLiquid, rb_intern("Lexer"));
    rb_define_singleton_method(cLiquidLexer, "c_lex", rb_lex, 1);
}
