#if !defined(LIQUID_H)
#define LIQUID_H

#include <ruby.h>
#include <ruby/encoding.h>
#include <stdbool.h>

#define TOKEN_STR(token) rb_enc_str_new((token).val, (token).val_end - (token).val, utf8_encoding)

extern VALUE mLiquid, cLiquidSyntaxError;
extern rb_encoding *utf8_encoding;

#endif

