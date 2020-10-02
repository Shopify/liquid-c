#include "liquid.h"
#include "tokenizer.h"
#include "variable.h"
#include "lexer.h"
#include "parser.h"
#include "resource_limits.h"
#include "block.h"
#include "context.h"
#include "variable_lookup.h"
#include "vm.h"

VALUE mLiquid, mLiquidC, cLiquidSyntaxError, cMemoryError, cLiquidVariable, cLiquidTemplate, cLiquidBlockBody;
rb_encoding *utf8_encoding;
int utf8_encoding_index;

__attribute__((noreturn)) void raise_non_utf8_encoding_error(const char *value_name)
{
    rb_raise(rb_eEncCompatError, "non-UTF8 encoded %s (ASCII-8BIT) not supported", value_name);
}

void Init_liquid_c(void)
{
    utf8_encoding = rb_utf8_encoding();
    utf8_encoding_index = rb_enc_to_index(utf8_encoding);

    mLiquid = rb_define_module("Liquid");
    rb_global_variable(&mLiquid);

    mLiquidC = rb_define_module_under(mLiquid, "C");
    rb_global_variable(&mLiquidC);

    cLiquidSyntaxError = rb_const_get(mLiquid, rb_intern("SyntaxError"));
    rb_global_variable(&cLiquidSyntaxError);

    cMemoryError = rb_const_get(mLiquid, rb_intern("MemoryError"));
    rb_global_variable(&cMemoryError);

    cLiquidVariable = rb_const_get(mLiquid, rb_intern("Variable"));
    rb_global_variable(&cLiquidVariable);

    cLiquidTemplate = rb_const_get(mLiquid, rb_intern("Template"));
    rb_global_variable(&cLiquidTemplate);

    cLiquidBlockBody = rb_const_get(mLiquid, rb_intern("BlockBody"));
    rb_global_variable(&cLiquidBlockBody);

    init_liquid_tokenizer();
    init_liquid_parser();
    init_liquid_resource_limits();
    init_liquid_variable();
    init_liquid_block();
    init_liquid_context();
    init_liquid_variable_lookup();
    init_liquid_vm();
}

