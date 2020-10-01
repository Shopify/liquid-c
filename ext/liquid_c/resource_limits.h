#ifndef LIQUID_RESOURCE_LIMITS
#define LIQUID_RESOURCE_LIMITS

typedef struct resource_limits {
    VALUE render_length_limit;
    VALUE render_score_limit;
    VALUE assign_score_limit;
    bool reached_limit;
    VALUE last_capture_length;
    VALUE render_score;
    VALUE assign_score;
} resource_limits_t;

extern VALUE cLiquidResourceLimits;
extern const rb_data_type_t resource_limits_data_type;
#define ResourceLimits_Get_Struct(obj, sval) TypedData_Get_Struct(obj, resource_limits_t, &resource_limits_data_type, sval)

void init_liquid_resource_limits();

#endif
