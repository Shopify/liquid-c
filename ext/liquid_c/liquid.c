#include "liquid.h"

VALUE mLiquid;

void Init_liquid_c(void)
{
    mLiquid = rb_define_module("Liquid");
    init_liquid_tokenizer();
}
