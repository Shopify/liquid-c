#include "liquid.h"
#include "context.h"

static ID id_has_key, id_aref, id_fetch;
static ID id_ivar_lookups, id_ivar_name, id_ivar_command_flags;

VALUE variable_lookup_evaluate(VALUE self, VALUE context)
{
    long command_flags = -1;
    VALUE name = rb_ivar_get(self, id_ivar_name);
    name = context_evaluate(context, name);

    VALUE object = context_find_variable(context, name, Qtrue);

    VALUE lookups = rb_ivar_get(self, id_ivar_lookups);
    Check_Type(lookups, T_ARRAY);

    for (long i = 0; i < RARRAY_LEN(lookups); i++) {
        VALUE key = context_evaluate(context, RARRAY_AREF(lookups, i));
        VALUE next_object;

        if (rb_respond_to(object, id_aref) && (
            (rb_respond_to(object, id_has_key) && rb_funcall(object, id_has_key, 1, key)) ||
            (rb_obj_is_kind_of(key, rb_cInteger) && rb_respond_to(object, id_fetch))
        )) {
            next_object = rb_funcall(object, id_aref, 1, key);
            next_object = materialize_proc(context, object, key, next_object);
            object = value_to_liquid_and_set_context(next_object, context);
            continue;
        }

        if (command_flags == -1) {
            command_flags = NUM2LONG(rb_ivar_get(self, id_ivar_command_flags));
        }
        if (command_flags & (1 << i)) {
            Check_Type(key, T_STRING);
            ID intern_key = rb_intern(RSTRING_PTR(key));
            if (rb_respond_to(object, intern_key)) {
                next_object = rb_funcall(object, rb_intern(RSTRING_PTR(key)), 0);
                object = value_to_liquid_and_set_context(next_object, context);
                continue;
            }
        }

        context_maybe_raise_undefined_variable(context, key);
        return Qnil;
    }

    return object;
}

void init_liquid_variable_lookup()
{
    id_has_key = rb_intern("key?");
    id_aref = rb_intern("[]");
    id_fetch = rb_intern("fetch");

    id_ivar_lookups = rb_intern("@lookups");
    id_ivar_name = rb_intern("@name");
    id_ivar_command_flags = rb_intern("@command_flags");

    VALUE cLiquidVariableLookup = rb_const_get(mLiquid, rb_intern("VariableLookup"));
    rb_global_variable(&cLiquidVariableLookup);
    rb_define_method(cLiquidVariableLookup, "c_evaluate", variable_lookup_evaluate, 1);
}
