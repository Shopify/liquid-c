#if !defined(LIQUID_CONTEXT_H)
#define LIQUID_CONTEXT_H

void init_liquid_context();
VALUE context_evaluate(VALUE self, VALUE expression);
VALUE context_find_variable(VALUE self, VALUE key, VALUE raise_on_not_found);
void context_maybe_raise_undefined_variable(VALUE self, VALUE key);

extern ID id_call, id_to_liquid, id_set_context;

inline static VALUE value_to_liquid_and_set_context(VALUE value, VALUE context_to_set)
{
    if (RB_SPECIAL_CONST_P(value))
        return value;

    VALUE klass = RBASIC(value)->klass;
    if (klass == rb_cString || klass == rb_cArray || klass == rb_cHash)
        return value;

    value = rb_funcall(value, id_to_liquid, 0);

    if (rb_respond_to(value, id_set_context))
        rb_funcall(value, id_set_context, 1, context_to_set);

    return value;
}


inline static VALUE materialize_proc(VALUE context, VALUE scope, VALUE key, VALUE value)
{
    if (scope != Qnil && !RB_SPECIAL_CONST_P(value) && TYPE(scope) == T_HASH) {
        if (rb_obj_is_kind_of(value, rb_cProc)) {
            if (rb_proc_arity(value) == 1) {
                value = rb_funcall(value, id_call, 1, context);
            } else {
                value = rb_funcall(value, id_call, 0);
            }
            rb_hash_aset(scope, key, value);
        }
    }
    return value;
}

#endif

