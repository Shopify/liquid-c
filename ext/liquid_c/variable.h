#if !defined(LIQUID_VARIABLE_H)
#define LIQUID_VARIABLE_H

#include "vm_assembler.h"

typedef struct variable_parse_args {
    const char *markup;
    const char *markup_end;
    vm_assembler_t *code;
    VALUE parse_context;
    unsigned int line_number;
} variable_parse_args_t;

void init_liquid_variable(void);
void internal_variable_parse(variable_parse_args_t *parse_args);

#endif

