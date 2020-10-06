#ifndef LIQUID_RAW_H
#define LIQUID_RAW_H

#include "c_buffer.h"

typedef struct raw {
    VALUE tag_name;
    VALUE parse_context;
    c_buffer_t body;
} raw_t;

extern VALUE cLiquidRaw;
extern const rb_data_type_t raw_data_type;
#define Raw_Get_Struct(obj, sval) TypedData_Get_Struct(obj, raw_t, &raw_data_type, sval)

void init_liquid_raw();

#endif
