#if !defined(LIQUID_H)
#define LIQUID_H

#include <ruby.h>
#include <ruby/encoding.h>
#include <stdbool.h>

extern VALUE mLiquid, mLiquidC, cLiquidSyntaxError, cLiquidVariable, cLiquidTemplate, cLiquidBlockBody;
extern rb_encoding *utf8_encoding;
extern int utf8_encoding_index;

__attribute__((noreturn)) void raise_non_utf8_encoding_error(const char *string_name);

inline void check_utf8_encoding(VALUE string, const char *string_name)
{
    if (RB_UNLIKELY(RB_ENCODING_GET_INLINED(string) != utf8_encoding_index))
        raise_non_utf8_encoding_error(string_name);
}

#ifndef RB_LIKELY
// RB_LIKELY added in Ruby 2.4
#define RB_LIKELY(x) (__builtin_expect(!!(x), 1))
#endif

#endif

