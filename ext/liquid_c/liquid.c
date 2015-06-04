#include "liquid.h"
#include "tokenizer.h"
#include "variable.h"
#include "lexer.h"
#include "parser.h"
#include "block.h"

VALUE mLiquid, mLiquidC, cLiquidSyntaxError, cLiquidVariable, cLiquidTemplate;
rb_encoding *utf8_encoding;

void Init_liquid_c(void)
{
    utf8_encoding = rb_utf8_encoding();
    mLiquid = rb_define_module("Liquid");
    mLiquidC = rb_define_module_under(mLiquid, "C");
    cLiquidSyntaxError = rb_const_get(mLiquid, rb_intern("SyntaxError"));
    cLiquidVariable = rb_const_get(mLiquid, rb_intern("Variable"));
    cLiquidTemplate = rb_const_get(mLiquid, rb_intern("Template"));

    init_liquid_tokenizer();
    init_liquid_parser();
    init_liquid_variable();
    init_liquid_block();
}

