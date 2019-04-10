#include "liquid.h"

static VALUE cLiquidVariableLookup;
static ID idEvaluate;

static VALUE context_evaluate(VALUE self, VALUE expression)
{
    // Scalar type stored directly in the VALUE
    if (RB_SPECIAL_CONST_P(expression))
        return expression;

    VALUE klass = RBASIC(expression)->klass;

    // Basic types that do not respond to #evaluate
    if (klass == rb_cString || klass == rb_cArray || klass == rb_cHash)
        return expression;

    // Liquid::VariableLookup is by far the most common type after String
    if (klass == cLiquidVariableLookup || rb_respond_to(expression, idEvaluate))
        return rb_funcall(expression, idEvaluate, 1, self);

    return expression;
}

void init_liquid_context()
{
    idEvaluate = rb_intern("evaluate");
    cLiquidVariableLookup = rb_const_get(mLiquid, rb_intern("VariableLookup"));

    VALUE cLiquidContext = rb_const_get(mLiquid, rb_intern("Context"));
    rb_define_method(cLiquidContext, "c_evaluate", context_evaluate, 1);
}
