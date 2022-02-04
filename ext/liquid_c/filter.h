static inline VALUE filter_split(VALUE *args)
{
    const char *delim = StringValuePtr(args[1]);
    VALUE output = rb_str_split(args[0], delim);

    return output;
}
