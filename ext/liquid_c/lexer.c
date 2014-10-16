#include "liquid.h"
#include "lexer.h"
#include "tokenizer.h"
#include <stdio.h>

static VALUE symbols[256] = {0};

static VALUE get_rb_type(unsigned char type) {
    VALUE ret = symbols[type];
    if (ret) return ret;

    ret = ID2SYM(rb_intern(symbol_names[type]));
    symbols[type] = ret;
    return ret;
}

static VALUE cLiquidSyntaxError;

static void raise_error(char unexpected) {
    rb_raise(cLiquidSyntaxError, "Unexpected character %c", unexpected);
}

static void append_token(lexer_token_list_t *tokens, unsigned char type, const char *val, unsigned int val_len) {
    if (tokens->len >= tokens->cap) {
        tokens->cap <<= 1;
        tokens->list = realloc(tokens->list, tokens->cap * sizeof(lexer_token_t));
    }

    lexer_token_t token;
    token.type = type;
    token.val = val;
    token.val_len = val_len;
    tokens->list[tokens->len++] = token;
}

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

inline static char is_white(unsigned char c) {
    return char_lookup[(unsigned)c] == 1;
}

inline static char is_digit(char c) {
    return c >= '0' && c <= '9';
}

inline static char is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline static char is_identifier(char c) {
    return is_digit(c) || is_alpha(c) || c == '_' || c == '-' || c == '?' || c == '!';
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
    while (cur < end && is_white(*cur)) ++cur;
    return cur;
}

// Returns a pointer to the character after the end of the match.
inline static const char *matches(const char *cur, const char *end, const char *pattern) {
    while (cur < end && *pattern != '\0') {
        if (*cur != *pattern) return 0;
        cur++;
        pattern++;
    }
    return *pattern == '\0' ? cur : 0; // True if the entire pattern was matched.
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

#define PUSH_TOKEN(type, n) { \
                                append_token(tokens, (type), start, (n)); \
                                return start + (n); \
                            }

const char *lex_one(const char *str, const char *end, lexer_token_list_t *tokens) {
    str = skip_white(str, end);
    if (str >= end) return str;

    const char *start = str;
    char c = *str, cn = 0;

    if (++str < end) cn = *str;

    // Comparison.
    if (c == '<') {
        if (cn == '>' || cn == '=') {
            PUSH_TOKEN(TOKEN_COMPARISON, 2);
        } else {
            PUSH_TOKEN(TOKEN_COMPARISON, 1);
        }
    } else if (c == '>') {
        PUSH_TOKEN(TOKEN_COMPARISON, cn == '=' ? 2 : 1);
    } else if ((c == '=' || c == '!') && cn == '=') {
        PUSH_TOKEN(TOKEN_COMPARISON, 2);
    } else if ((str = matches(start, end, "contains"))) {
        PUSH_TOKEN(TOKEN_COMPARISON, str - start);
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

        if (str) PUSH_TOKEN(TOKEN_QUOTE, str - start);
    }

    // Numbers.
    if (is_digit(c) || c == '-') {
        char has_dot = 0;
        str = start;
        while (++str < end) {
            cn = *str;
            if (!has_dot && cn == '.') {
                has_dot = 1;
            } else if (!is_digit(cn)) {
                break;
            }
        }
        if (cn == '.') str--;
        PUSH_TOKEN(TOKEN_NUMBER, str - start);
    }

    // Identifiers.
    if (is_identifier(c)) {
        str = start;
        while (++str < end && is_identifier(*str)) {}
        PUSH_TOKEN(TOKEN_IDENTIFIER, str - start);
    }

    // Double dots.
    if (c == '.' && cn == '.') {
        PUSH_TOKEN(TOKEN_DOTDOT, 2);
    }

    // Specials.
    if (is_special(c)) {
        PUSH_TOKEN(c, 1);
    }

    raise_error(c);
    return NULL;
}

#undef PUSH_TOKEN

void lex(const char *start, const char *end, lexer_token_list_t *tokens)
{
    const char *cur = start;

    while (cur < end) {
        cur = lex_one(cur, end, tokens);
    }

    append_token(tokens, TOKEN_EOS, NULL, 0);
}

VALUE rb_lex(VALUE self, VALUE markup) {
    lexer_token_list_t *tokens = new_token_list();

    const char *str = RSTRING_PTR(markup);
    lex(str, str + RSTRING_LEN(markup), tokens);

    VALUE output = rb_ary_new();
    lexer_token_t *cur = tokens->list;

    for (int i = 0; i < tokens->len; i++) {
        // fprintf(stderr, "%s:%.*s, ", symbol_names[cur[i].type], cur[i].val_len, cur[i].val);

        VALUE rb_token = rb_ary_new();
        rb_ary_push(rb_token, get_rb_type(cur[i].type));

        if (cur[i].val) {
            rb_ary_push(rb_token, rb_str_new(cur[i].val, cur[i].val_len));
        }

        rb_ary_push(output, rb_token);
    }

    return output;
}

lexer_token_t *consume(lexer_token_list_t *tokens, unsigned char type)
{
    if (tokens->len <= 0) {
        rb_raise(cLiquidSyntaxError, "Expected %s but found nothing", symbol_names[type]);
    }

    lexer_token_t *token = tokens->list++;
    if (token->type != type) {
        rb_raise(cLiquidSyntaxError, "Expected %s but found %s", symbol_names[type], symbol_names[token->type]);
    }

    return token;
}

lexer_token_list_t *new_token_list()
{
    lexer_token_list_t *tokens = malloc(sizeof(lexer_token_list_t));
    tokens->len = 0;
    tokens->cap = 4;
    tokens->list = malloc(4 * sizeof(lexer_token_t));
    return tokens;
}

void init_liquid_lexer(void)
{
    cLiquidSyntaxError = rb_const_get(mLiquid, rb_intern("SyntaxError"));
    rb_define_singleton_method(mLiquid, "c_lex", rb_lex, 1);
}
