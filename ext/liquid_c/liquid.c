#include "liquid.h"
#include "tokenizer.h"
#include "variable.h"

VALUE mLiquid;
rb_encoding *utf8_encoding;

void Init_liquid_c(void)
{
    utf8_encoding = rb_utf8_encoding();
    mLiquid = rb_define_module("Liquid");
    init_liquid_tokenizer();
    init_liquid_variable();
}
