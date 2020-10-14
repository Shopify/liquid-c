#if !defined(LIQUID_BLOCK_H)
#define LIQUID_BLOCK_H

typedef struct block_body {
    size_t instructions_offset;
    size_t instructions_bytes;
    size_t constants_offset;
    size_t constants_bytes;
    bool blank;
    int render_score;
    size_t max_stack_size;
} block_body_t;

void init_liquid_block();

static inline VALUE *block_body_constants_ptr(block_body_t *body)
{
    return (VALUE *)(((char *)body) + body->constants_offset);
}

static inline uint8_t *block_body_instructions_ptr(block_body_t *body)
{
    return (uint8_t *)(((char *)body) + body->instructions_offset);
}

#endif
