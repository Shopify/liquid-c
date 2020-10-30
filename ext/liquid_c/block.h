#if !defined(LIQUID_BLOCK_H)
#define LIQUID_BLOCK_H

#include "vm_assembler_pool.h"

typedef struct block_body_header {
    uint32_t instructions_offset;
    uint32_t instructions_bytes;
    uint32_t constants_offset;
    uint32_t constants_len;
    bool blank;
    int render_score;
    size_t max_stack_size;
} block_body_header_t;

typedef struct block_body {
    bool compiled;
    VALUE obj;

    union {
        struct {
            VALUE document_body_obj;
            c_buffer_t *buffer;
            size_t offset;
            VALUE constants;
            VALUE nodelist;
        } compiled;
        struct {
            VALUE parse_context;
            vm_assembler_pool_t *vm_assembler_pool;
            bool blank;
            bool root;
            int render_score;
            vm_assembler_t *code;
        } intermediate;
    } as;
} block_body_t;

void init_liquid_block();

static inline uint8_t *block_body_instructions_ptr(block_body_header_t *body)
{
    return ((uint8_t *)body) + body->instructions_offset;
}

#endif
