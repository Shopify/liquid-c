#include "immintrin.h"

#if !defined(LIQUID_UTIL_H)
#define LIQUID_UTIL_H

char* vcompare2(char* cursor, char needle) {
    const __m256i haystack = _mm256_loadu_si256((const __m256i*)cursor);
    const __m256i needles = _mm256_set1_epi8(needle);
    const __m256i eq = _mm256_cmpeq_epi8(haystack, needles);
    int result = _mm256_movemask_epi8(eq);

    return cursor + __builtin_ctz(result);
}

char* block_search(char* cursor, char* last, char needle) {
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

