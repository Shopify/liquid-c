#include "liquid.h"
#include "vm_assembler.h"
#include "block.h"
#include "tokenizer.h"
#include "variable.h"

static ID id_parse;

static VALUE cLiquidTag, cLiquidAssign;
static VALUE liquid_assign_syntax;

static VALUE tag_class_compile(VALUE klass, VALUE tag_name, VALUE markup,
        VALUE tokenizer_obj, VALUE parse_context_obj, VALUE block_body_obj)
{
    block_body_t *body;
    BlockBody_Get_Struct(block_body_obj, body);

    VALUE new_tag = rb_funcall(klass, id_parse, 4, tag_name, markup,
            tokenizer_obj, parse_context_obj);

    block_body_ensure_intermediate(body);

    if (body->as.intermediate.blank && !RTEST(rb_funcall(new_tag, id_blank_p, 0)))
        body->as.intermediate.blank = false;

    rb_funcall(new_tag, id_compile, 1, block_body_obj);

    return Qnil;
}

static VALUE tag_compile(VALUE self, VALUE block_body_obj)
{
    block_body_t *body;
    BlockBody_Get_Struct(block_body_obj, body);
    block_body_ensure_intermediate(body);
    vm_assembler_add_write_node_from_ruby(body->as.intermediate.code, self);
    return Qnil;
}

static VALUE echo_class_compile(VALUE klass, VALUE tag_name, VALUE markup,
        VALUE tokenizer_obj, VALUE parse_context_obj, VALUE block_body_obj)
{
    block_body_t *body;
    BlockBody_Get_Struct(block_body_obj, body);
    block_body_ensure_intermediate(body);

    tokenizer_t *tokenizer;
    Tokenizer_Get_Struct(tokenizer_obj, tokenizer);

    variable_parse_args_t parse_args = {
        .markup = RSTRING_PTR(markup),
        .markup_end = RSTRING_PTR(markup) + RSTRING_LEN(markup),
        .code = body->as.intermediate.code,
        .code_obj = block_body_obj,
        .parse_context = parse_context_obj,
    };
    internal_variable_compile(&parse_args, tokenizer->line_number);
    return Qnil;
}

static VALUE assign_class_compile(VALUE klass, VALUE tag_name, VALUE markup,
        VALUE tokenizer_obj, VALUE parse_context_obj, VALUE block_body_obj)
{
    block_body_t *body;
    BlockBody_Get_Struct(block_body_obj, body);
    block_body_ensure_intermediate(body);

    if (rb_reg_match(liquid_assign_syntax, markup) == Qnil)
        rb_funcall(cLiquidAssign, rb_intern("raise_syntax_error"), 1, parse_context_obj);
    VALUE last_match = rb_backref_get();
    VALUE var_name = rb_reg_nth_match(1, last_match);
    VALUE var_markup = rb_reg_nth_match(2, last_match);

    variable_parse_args_t parse_args = {
        .markup = RSTRING_PTR(var_markup),
        .markup_end = RSTRING_END(var_markup),
        .code = body->as.intermediate.code,
        .code_obj = block_body_obj,
        .parse_context = parse_context_obj,
    };
    internal_variable_compile_evaluate(&parse_args);
    vm_assembler_add_assign(body->as.intermediate.code, var_name);
    return Qnil;
}

void liquid_define_tag()
{
    id_parse = rb_intern("parse");

    cLiquidTag = rb_const_get(mLiquid, rb_intern("Tag"));
    rb_global_variable(&cLiquidTag);

    rb_define_singleton_method(cLiquidTag, "compile", tag_class_compile, 5);
    rb_define_method(cLiquidTag, "compile", tag_compile, 1);

    VALUE cLiquidEcho = rb_const_get(mLiquid, rb_intern("Echo"));
    rb_define_singleton_method(cLiquidEcho, "compile", echo_class_compile, 5);

    cLiquidAssign = rb_const_get(mLiquid, rb_intern("Assign"));
    rb_global_variable(&cLiquidAssign);
    rb_define_singleton_method(cLiquidAssign, "compile", assign_class_compile, 5);

    liquid_assign_syntax = rb_const_get(cLiquidAssign, rb_intern("Syntax"));
    rb_global_variable(&liquid_assign_syntax);
    Check_Type(liquid_assign_syntax, T_REGEXP);
}
