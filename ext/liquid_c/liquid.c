#include "liquid.h"
#include "tokenizer.h"
#include "variable.h"
#include "lexer.h"

VALUE mLiquid, cLiquidSyntaxError;
rb_encoding *utf8_encoding;

void Init_liquid_c(void)
{
    utf8_encoding = rb_utf8_encoding();
    mLiquid = rb_define_module("Liquid");
    cLiquidSyntaxError = rb_const_get(mLiquid, rb_intern("SyntaxError"));
    init_liquid_tokenizer();
    init_liquid_variable();
    init_liquid_lexer();
}
