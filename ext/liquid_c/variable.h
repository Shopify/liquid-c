#if !defined(LIQUID_VARIABLE_H)
#define LIQUID_VARIABLE_H

#include "liquid.h"

typedef struct variable_parser {
    VALUE name;
    VALUE filters;
} variable_parser_t;

extern VALUE cLiquidVariableParser;
extern const rb_data_type_t variable_parser_data_type;
#define Variable_Parser_Get_Struct(obj, sval) TypedData_Get_Struct(obj, \
        variable_parser_t, &variable_parser_data_type, sval)

void init_liquid_variable(void);

#endif
