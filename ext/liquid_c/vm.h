#ifndef VM_H
#define VM_H

#include <ruby.h>
#include "block.h"

enum opcode {
    OP_LEAVE = 0,
    OP_WRITE_RAW = 1,
    OP_WRITE_NODE = 2,
};

void init_liquid_vm();
void liquid_vm_render(block_body_t *block, VALUE context, VALUE output);

#endif
