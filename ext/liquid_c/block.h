#if !defined(LIQUID_BLOCK_H)
#define LIQUID_BLOCK_H

typedef struct block_body_header {
    uint32_t instructions_offset;
    uint32_t instructions_bytes;
    uint32_t constants_offset;
    uint32_t constants_len;
    bool blank;
    int render_score;
    size_t max_stack_size;
} block_body_header_t;

void init_liquid_block();

static inline uint8_t *block_body_instructions_ptr(block_body_header_t *body)
{
    return ((uint8_t *)body) + body->instructions_offset;
}

#endif
