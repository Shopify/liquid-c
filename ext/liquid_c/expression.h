#if !defined(LIQUID_EXPRESSION_H)
#define LIQUID_EXPRESSION_H

#include "vm_assembler.h"
#include "parser.h"

extern VALUE cLiquidCExpression;

typedef struct expression {
    vm_assembler_t code;
} expression_t;

void init_liquid_expression();

VALUE expression_new(expression_t **expression_ptr);
VALUE internal_expression_evaluate(expression_t *expression, VALUE context);

#endif

