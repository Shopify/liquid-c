#ifndef VM_H
#define VM_H

#include <ruby.h>
#include "block.h"
#include "vm_assembler.h"
#include "context.h"

typedef struct vm {
    c_buffer_t stack;
    bool invoking_filter;
    context_t context;
} vm_t;

void liquid_define_vm();
vm_t *vm_from_context(VALUE context);
void liquid_vm_render(block_body_header_t *body, const VALUE *const_ptr, const VALUE *tags_ptr, VALUE context, VALUE output);
void liquid_vm_next_instruction(const uint8_t **ip_ptr, const size_t **const_ptr_ptr, const VALUE **tags_ptr_ptr);
bool liquid_vm_filtering(VALUE context);
VALUE liquid_vm_evaluate(VALUE context, vm_assembler_t *code);

vm_t *vm_from_context(VALUE context);
VALUE vm_translate_if_filter_argument_error(vm_t *vm, VALUE exception);

#endif
