#if !defined(LIQUID_H)
#define LIQUID_H

#include <ruby.h>
#include <ruby/encoding.h>
#include <stdbool.h>

extern VALUE mLiquid, mLiquidC, cLiquidSyntaxError, cLiquidVariable, cLiquidTemplate;
extern rb_encoding *utf8_encoding;

#endif

