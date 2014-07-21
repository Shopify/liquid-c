#if !defined(LIQUID_PARSE_H)
#define LIQUID_PARSE_H

#include <stddef.h>

/*
 * See parse.c for description of the character types
 */
extern const unsigned char char_lookup[256];

inline static int is_white(char c) {
    return char_lookup[(unsigned)c] == 1;
}

inline static int is_word_char(char c) {
    return char_lookup[(unsigned)c] == 2;
}

inline static int is_quote_delimiter(char c) {
    return char_lookup[(unsigned)c] == 4;
}

inline static int is_quoted_fragment_terminator(char c) {
    return char_lookup[(unsigned)c] & 1;
}

inline static const char *scan_past(const char *cur, const char *end, char target) {
    ++cur;
    while (cur < end && *cur != target) ++cur;
    if (cur >= end) return NULL;
    ++cur;
    return cur;
}

inline static const char *scan_word(const char *cur, const char *end) {
    while (cur < end && is_word_char(*cur)) ++cur;
    return cur;
}

inline static const char *skip_white(const char *cur, const char *end) {
    while (cur < end && is_white(*cur)) ++cur;
    return cur;
}

inline static int all_blank(const char *str, const char *end) {
    const char *i;
    for (i = str; i < end; ++i) {
        if (!is_white(*i)) return 0;
    }
    return 1;
}

#endif
