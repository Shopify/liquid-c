#if !defined(LIQUID_H)
#define LIQUID_H

#include <ruby.h>
#include <ruby/encoding.h>
#include <stdbool.h>

#define rb_utf8_str_new_range(start, end) rb_enc_str_new((start), (end - start), utf8_encoding)

extern VALUE mLiquid;
extern rb_encoding *utf8_encoding;

#endif
