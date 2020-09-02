#if !defined(LIQUID_BLOCK_H)
#define LIQUID_BLOCK_H

#include "c_buffer.h"

typedef struct block_body {
    c_buffer_t instructions;
    c_buffer_t constants;
    VALUE source; // hold a reference to the ruby object that OP_WRITE_RAW points to
    bool parsing; // use to prevent rendering when parsing is incomplete
    bool blank;
    int render_score;
} block_body_t;

void init_liquid_block();

#endif

