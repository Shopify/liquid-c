#ifndef VM_H
#define VM_H

#include <ruby.h>
#include "block.h"

void init_liquid_vm();
void liquid_vm_render(block_body_t *block, VALUE context, VALUE output);
void liquid_vm_next_instruction(const uint8_t **ip_ptr, const size_t **const_ptr_ptr);

#endif
