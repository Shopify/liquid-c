#ifndef LIQUID_PARSE_CONTEXT_H
#define LIQUID_PARSE_CONTEXT_H

#include <ruby.h>
#include <stdbool.h>
#include "vm_assembler_pool.h"

void init_liquid_parse_context();
bool parse_context_document_body_initialized_p(VALUE self);
void parse_context_init_document_body(VALUE self);
VALUE parse_context_get_document_body(VALUE self);
void parse_context_remove_document_body(VALUE self);

vm_assembler_pool_t *parse_context_init_vm_assembler_pool(VALUE self);
vm_assembler_pool_t *parse_context_get_vm_assembler_pool(VALUE self);
void parse_context_remove_vm_assembler_pool(VALUE self);

#endif
