static inline VALUE filter_split(VALUE *args, int argc)
{
    if (argc != 2) {
        rb_raise(cLiquidArgumentError, "wrong number of arguments (given %i, expected 2)", argc);

        return Qnil;
    }

    const char *delim = StringValuePtr(args[1]);
    VALUE output = rb_str_split(args[0], delim);

    return output;
}
