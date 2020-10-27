#ifndef LIQUID_PARSE_CONTEXT_H
#define LIQUID_PARSE_CONTEXT_H

#include <ruby.h>
#include <stdbool.h>

void init_liquid_parse_context();
bool parse_context_init_document_body(VALUE self);
VALUE parse_context_get_document_body(VALUE self);
void parse_context_remove_document_body(VALUE self);

#endif
