#include "liquid.h"
#include "context.h"
#include "variable_lookup.h"

static VALUE cLiquidVariableLookup, cLiquidUndefinedVariable;
ID id_call, id_to_liquid, id_set_context;
static ID id_evaluate, id_has_key, id_aref;
static ID id_ivar_scopes, id_ivar_environments, id_ivar_strict_variables;

VALUE context_evaluate(VALUE self, VALUE expression)
{
    // Scalar type stored directly in the VALUE
    if (RB_SPECIAL_CONST_P(expression))
        return expression;

    VALUE klass = RBASIC(expression)->klass;

    // Basic types that do not respond to #evaluate
    if (klass == rb_cString || klass == rb_cArray || klass == rb_cHash)
        return expression;

    // Liquid::VariableLookup is by far the most common type after String
    if (klass == cLiquidVariableLookup)
        return variable_lookup_evaluate(expression, self);

    if (rb_respond_to(expression, id_evaluate))
        return rb_funcall(expression, id_evaluate, 1, self);

    return expression;
}

void context_maybe_raise_undefined_variable(VALUE self, VALUE key)
{
    if (rb_ivar_get(self, id_ivar_strict_variables)) {
        Check_Type(key, T_STRING);
        rb_enc_raise(utf8_encoding, cLiquidUndefinedVariable, "undefined variable %s", RSTRING_PTR(key));
    }
}

VALUE context_find_variable(VALUE self, VALUE key, VALUE raise_on_not_found)
{
    VALUE scope = Qnil, variable = Qnil;

    VALUE scopes = rb_ivar_get(self, id_ivar_scopes);
    Check_Type(scopes, T_ARRAY);
    const VALUE *scopes_ptr = RARRAY_CONST_PTR(scopes);
    long scopes_len = RARRAY_LEN(scopes);

    for (long i = 0; i < scopes_len; i++) {
        VALUE this_scope = scopes_ptr[i];
        if (TYPE(this_scope) == T_HASH) {
            variable = rb_hash_lookup2(this_scope, key, Qundef);
            if (variable != Qundef) {
                scope = this_scope;
                goto variable_found;
            }
        } else if (RTEST(rb_funcall(this_scope, id_has_key, 1, key))) {
            variable = rb_funcall(this_scope, id_aref, 1, key);
            goto variable_found;
        }
    }

    VALUE environments = rb_ivar_get(self, id_ivar_environments);
    Check_Type(environments, T_ARRAY);
    const VALUE *environments_ptr = RARRAY_CONST_PTR(environments);
    long environments_len = RARRAY_LEN(environments);

    for (long i = 0; i < environments_len; i++) {
        VALUE this_environ = environments_ptr[i];
        if (TYPE(this_environ) == T_HASH) {
            variable = rb_hash_lookup2(this_environ, key, Qundef);
            if (variable != Qundef) {
                scope = this_environ;
                goto variable_found;
            }
        } else if (RTEST(rb_funcall(this_environ, id_has_key, 1, key))) {
            variable = rb_funcall(this_environ, id_aref, 1, key);
            goto variable_found;
        }
    }

    if (RTEST(raise_on_not_found)) {
        context_maybe_raise_undefined_variable(self, key);
    }
    variable = Qnil;

variable_found:
    variable = materialize_proc(self, scope, key, variable);
    variable = value_to_liquid_and_set_context(variable, self);

    return variable;
}

void init_liquid_context()
{
    id_evaluate = rb_intern("evaluate");
    id_call = rb_intern("call");
    id_has_key = rb_intern("key?");
    id_aref = rb_intern("[]");
    id_to_liquid = rb_intern("to_liquid");
    id_set_context = rb_intern("context=");

    id_ivar_scopes = rb_intern("@scopes");
    id_ivar_environments = rb_intern("@environments");
    id_ivar_strict_variables = rb_intern("@strict_variables");

    cLiquidVariableLookup = rb_const_get(mLiquid, rb_intern("VariableLookup"));
    cLiquidUndefinedVariable = rb_const_get(mLiquid, rb_intern("UndefinedVariable"));

    VALUE cLiquidContext = rb_const_get(mLiquid, rb_intern("Context"));
    rb_define_method(cLiquidContext, "c_evaluate", context_evaluate, 1);
    rb_define_method(cLiquidContext, "c_find_variable", context_find_variable, 2);
}
