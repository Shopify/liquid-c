#if !defined(LIQUID_PARSE_H)
#define LIQUID_PARSE_H

#include <stddef.h>

/*
 * See parse.c for description of the character types
 */
extern const unsigned char char_lookup[256];

inline static int is_white(unsigned char c) {
    return char_lookup[(unsigned)c] == 1;
}

inline static int is_word_char(unsigned char c) {
    return char_lookup[(unsigned)c] == 2;
}

inline static int is_quote_delimiter(unsigned char c) {
    return char_lookup[(unsigned)c] == 4;
}

inline static int is_quoted_fragment_terminator(unsigned char c) {
    return char_lookup[(unsigned)c] & 1;
}

inline static const unsigned char *scan_past(const unsigned char *cur, const unsigned char *end, unsigned char target) {
    ++cur;
    while (cur < end && *cur != target) ++cur;
    if (cur >= end) return NULL;
    ++cur;
    return cur;
}

inline static const unsigned char *scan_word(const unsigned char *cur, const unsigned char *end) {
    while (cur < end && is_word_char(*cur)) ++cur;
    return cur;
}

inline static const unsigned char *skip_white(const unsigned char *cur, const unsigned char *end) {
    while (cur < end && is_white(*cur)) ++cur;
    return cur;
}

inline static int all_blank(const unsigned char *str, const unsigned char *end) {
    const unsigned char *i;
    for (i = str; i < end; ++i) {
        if (!is_white(*i)) return 0;
    }
    return 1;
}

#endif
