#include "immintrin.h"

#if !defined(LIQUID_UTIL_H)
#define LIQUID_UTIL_H

/*
inline static int is_identifier(char c)
{
    return ISALNUM(c) || c == '_' || c == '-';
}
while (++cur < end && is_identifier(*cur)) {}
*/
inline static const char* isidentifier(char* cursor) {
    const __m256i upperA = _mm256_set1_epi8('A' - 1);
    const __m256i upperZ = _mm256_set1_epi8('Z' + 1);
    const __m256i lowerA = _mm256_set1_epi8('a' - 1);
    const __m256i lowerZ = _mm256_set1_epi8('z' + 1);
    const __m256i digit0 = _mm256_set1_epi8('0' - 1);
    const __m256i digit9 = _mm256_set1_epi8('9' + 1);
    const __m256i haystack = _mm256_loadu_si256((const __m256i*)cursor);

    const __m256i geupperA = _mm256_cmpgt_epi8(haystack, upperA);
    const __m256i leupperZ = _mm256_cmpgt_epi8(upperZ, haystack);
    const __m256i gelowerA = _mm256_cmpgt_epi8(haystack, lowerA);
    const __m256i lelowerZ = _mm256_cmpgt_epi8(lowerZ, haystack);
    const __m256i gedigit0 = _mm256_cmpgt_epi8(haystack, digit0);
    const __m256i ledigit9= _mm256_cmpgt_epi8(digit9, haystack);

    int result = _mm256_movemask_epi8(geupperA & leupperZ);
    result |= _mm256_movemask_epi8(gelowerA & lelowerZ);
    result |= _mm256_movemask_epi8(gedigit0 & ledigit9);

    result = ~result;

    return cursor + __builtin_ctz(result);
}

inline static const char* vcompare2(char* cursor, char needle) {
    const __m256i haystack = _mm256_loadu_si256((const __m256i*)cursor);
    const __m256i needles = _mm256_set1_epi8(needle);
    const __m256i eq = _mm256_cmpeq_epi8(haystack, needles);
    int result = _mm256_movemask_epi8(eq);

    return cursor + __builtin_ctz(result);
}

inline static const char* block_search(char* cursor, char* last, char needle) {
    char* test;
    while((last - cursor + 1) >= 32) {
        test = vcompare2(cursor, needle);
        if(test != cursor + 32)
            return test;
        cursor = test;
    }
    while(cursor < last && *cursor != needle) {
        cursor++;
    }
    return cursor;
}

inline static const char *read_while_not_newline(const char *start, const char *end)
{
    return block_search(start, end, '\n');
}

inline static const char *read_while(const char *start, const char *end, int (func)(int))
{
    while (start < end && func((unsigned char) *start)) start++;
    return start;
}

inline static const char *read_while_reverse(const char *start, const char *end, int (func)(int))
{
    end--;
    while (start <= end && func((unsigned char) *end)) end--;
    end++;
    return end;
}

inline static int count_newlines(const char *start, const char *end)
{
    int count = 0;
    while (start < end) {
        if (*start == '\n') count++;
        start++;
    }
    return count;
}

inline static int is_non_newline_space(int c)
{
    return rb_isspace(c) && c != '\n';
}

inline static int not_newline(int c)
{
    return c != '\n';
}

inline static bool is_word_char(char c)
{
    return ISALNUM(c) || c == '_';
}

#endif

