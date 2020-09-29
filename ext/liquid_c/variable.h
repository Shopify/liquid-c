#if !defined(LIQUID_VARIABLE_H)
#define LIQUID_VARIABLE_H

#include "vm_assembler.h"
#include "block.h"

typedef struct variable_parse_args {
    const char *markup;
    const char *markup_end;
    block_body_t *body;
    VALUE parse_context;
} variable_parse_args_t;

void init_liquid_variable(void);
void internal_variable_compile(variable_parse_args_t *parse_args, unsigned int line_number);
void internal_variable_compile_evaluate(variable_parse_args_t *parse_args);

#endif

