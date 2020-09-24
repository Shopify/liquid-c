#if !defined(LIQUID_BLOCK_H)
#define LIQUID_BLOCK_H

#include "vm_assembler.h"

typedef struct block_body {
    VALUE obj;
    vm_assembler_t code;
    VALUE source; // hold a reference to the ruby object that OP_WRITE_RAW points to
    bool blank;
    int render_score;
} block_body_t;

void init_liquid_block();

#endif

