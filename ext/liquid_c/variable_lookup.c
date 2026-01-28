#include "liquid.h"
#include "context.h"

static ID id_has_key, id_aref, id_fetch, id_to_liquid_value;

/* Helper to check if key matches a string */
static inline bool key_eq(VALUE key, const char *str)
{
    if (!RB_TYPE_P(key, T_STRING)) return false;
    size_t len = strlen(str);
    return (size_t)RSTRING_LEN(key) == len && memcmp(RSTRING_PTR(key), str, len) == 0;
}

VALUE variable_lookup_key(VALUE context, VALUE object, VALUE key, bool is_command)
{
    if (rb_obj_class(key) != rb_cString) {
        VALUE key_value = rb_check_funcall(key, id_to_liquid_value, 0, 0);

        if (key_value != Qundef) {
            key = key_value;
        }
    }

    if (rb_respond_to(object, id_aref) && (
        (rb_respond_to(object, id_has_key) && rb_funcall(object, id_has_key, 1, key)) ||
        (rb_obj_is_kind_of(key, rb_cInteger) && rb_respond_to(object, id_fetch))
    )) {
        VALUE next_object = rb_funcall(object, id_aref, 1, key);
        next_object = materialize_proc(context, object, key, next_object);
        return value_to_liquid_and_set_context(next_object, context);
    }

    if (is_command) {
        Check_Type(key, T_STRING);

        /* Special handling for strings: first/last return first/last character */
        if (RB_TYPE_P(object, T_STRING)) {
            long len = RSTRING_LEN(object);
            if (key_eq(key, "first")) {
                if (len > 0) {
                    return rb_str_substr(object, 0, 1);
                }
                return Qnil;
            }
            if (key_eq(key, "last")) {
                if (len > 0) {
                    return rb_str_substr(object, len - 1, 1);
                }
                return Qnil;
            }
        }

        ID intern_key = rb_intern(RSTRING_PTR(key));
        if (rb_respond_to(object, intern_key)) {
            VALUE next_object = rb_funcall(object, intern_key, 0);
            return value_to_liquid_and_set_context(next_object, context);
        }
    }

    context_maybe_raise_undefined_variable(context, key);
    return Qnil;
}

void liquid_define_variable_lookup(void)
{
    id_has_key = rb_intern("key?");
    id_aref = rb_intern("[]");
    id_fetch = rb_intern("fetch");
    id_to_liquid_value = rb_intern("to_liquid_value");
}
