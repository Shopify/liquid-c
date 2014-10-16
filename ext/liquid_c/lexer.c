#include "liquid.h"
#include "lexer.h"
#include "tokenizer.h"
#include <stdio.h>

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

inline static char is_white(unsigned char c) {
    switch (c) {
        case ' ':
        case '\n': case '\r':
        case '\t': case '\f':
            return 1;
    }
    return 0;
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

#define PUSH_TOKEN(t, n) { \
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

void init_lexer(lexer_t *lexer, const char *str, const char *end) {
    lexer->str = str;
    lexer->str_end = end;
    lexer->cur.type = TOKEN_EOS;
    lexer->next.type = TOKEN_EOS;
    lexer->str = lex_one(lexer->str, lexer->str_end, &lexer->cur);
    lexer->str = lex_one(lexer->str, lexer->str_end, &lexer->next);
}

lexer_token_t lexer_consume_any(lexer_t *lexer) {
    lexer_token_t cur = lexer->cur;
    lexer->cur = lexer->next;
    lexer->next.type = TOKEN_EOS;
    lexer->str = lex_one(lexer->str, lexer->str_end, &lexer->next);
    return cur;
}

lexer_token_t lexer_must_consume(lexer_t *lexer, unsigned char type) {
    if (lexer->cur.type != type) {
        rb_raise(cLiquidSyntaxError, "Expected %s but found %s", symbol_names[type], symbol_names[lexer->cur.type]);
    }
    return lexer_consume_any(lexer);
}

lexer_token_t lexer_consume(lexer_t *lexer, unsigned char type) {
    if (lexer->cur.type != type) {
        lexer_token_t zero;
        zero.type = 0;
        return zero;
    }
    return lexer_consume_any(lexer);
}

VALUE lexer_expression(lexer_t *lexer) {
    lexer_token_t token;

    switch (lexer->cur.type) {
        case TOKEN_IDENTIFIER:
            return lexer_variable_signature(lexer);

        case TOKEN_STRING:
        case TOKEN_NUMBER:
            token = lexer_consume_any(lexer);
            return rb_utf8_str_new_range(token.val, token.val_end);

        case TOKEN_OPEN_ROUND:
        {
            lexer_consume_any(lexer);
            VALUE first = lexer_expression(lexer);
            lexer_must_consume(lexer, TOKEN_DOTDOT);
            VALUE last = lexer_expression(lexer);
            lexer_must_consume(lexer, TOKEN_CLOSE_ROUND);

            VALUE out = rb_enc_str_new("(", 1, utf8_encoding);
            rb_str_append(out, first);
            rb_str_append(out, rb_str_new2(".."));
            rb_str_append(out, last);
            rb_str_append(out, rb_str_new2(")"));
            return out;
        }
    }
    if (lexer->cur.type == TOKEN_EOS) {
        rb_raise(cLiquidSyntaxError, "[:%s] is not a valid expression", symbol_names[lexer->cur.type]);
    } else {
        rb_raise(cLiquidSyntaxError, "[:%s, \"%.*s\"] is not a valid expression",
                symbol_names[lexer->cur.type], (int)(lexer->cur.val_end - lexer->cur.val), lexer->cur.val);
    }
    return Qnil;
}

VALUE lexer_argument(lexer_t *lexer) {
    VALUE str = rb_enc_str_new("", 0, utf8_encoding);

    if (lexer->cur.type == TOKEN_IDENTIFIER && lexer->next.type == TOKEN_COLON) {
        lexer_token_t token = lexer_consume_any(lexer);
        rb_str_append(str, rb_utf8_str_new_range(token.val, token.val_end));

        lexer_consume_any(lexer);
        rb_str_append(str, rb_str_new2(": "));
    }

    return rb_str_append(str, lexer_expression(lexer));
}

VALUE lexer_variable_signature(lexer_t *lexer) {
    lexer_token_t token = lexer_must_consume(lexer, TOKEN_IDENTIFIER);
    VALUE str = rb_utf8_str_new_range(token.val, token.val_end);

    if (lexer->cur.type == TOKEN_OPEN_SQUARE) {
        token = lexer_consume_any(lexer);
        rb_str_append(str, rb_utf8_str_new_range(token.val, token.val_end));

        rb_str_append(str, lexer_expression(lexer));

        token = lexer_must_consume(lexer, TOKEN_CLOSE_SQUARE);
        rb_str_append(str, rb_utf8_str_new_range(token.val, token.val_end));
    }

    if (lexer->cur.type == TOKEN_DOT) {
        token = lexer_consume_any(lexer);
        rb_str_append(str, rb_utf8_str_new_range(token.val, token.val_end));

        rb_str_append(str, lexer_variable_signature(lexer));
    }

    return str;
}

VALUE rb_lex(VALUE self, VALUE markup) {
    const char *str = RSTRING_PTR(markup);
    const char *end = str + RSTRING_LEN(markup);

    lexer_token_t token;
    VALUE output = rb_ary_new();

    while (str < end) {
        token.type = 0;
        str = lex_one(str, end, &token);

        if (token.type) {
            VALUE rb_token = rb_ary_new();
            rb_ary_push(rb_token, get_rb_type(token.type));
            rb_ary_push(rb_token, rb_utf8_str_new_range(token.val, token.val_end));
            rb_ary_push(output, rb_token);
        }
    }

    VALUE rb_eos = rb_ary_new();
    rb_ary_push(rb_eos, get_rb_type(TOKEN_EOS));
    rb_ary_push(output, rb_eos);
    return output;
}

static VALUE cLiquidLexer;

void init_liquid_lexer(void)
{
    cLiquidLexer = rb_const_get(mLiquid, rb_intern("Lexer"));
    cLiquidSyntaxError = rb_const_get(mLiquid, rb_intern("SyntaxError"));
    rb_define_singleton_method(cLiquidLexer, "c_lex", rb_lex, 1);
}
