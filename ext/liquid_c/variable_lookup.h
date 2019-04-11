#if !defined(LIQUID_VARIABLE_LOOKUP_H)
#define LIQUID_VARIABLE_LOOKUP_H

void init_liquid_variable_lookup();
VALUE variable_lookup_evaluate(VALUE self, VALUE expression);

#endif

