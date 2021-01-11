#if !defined(LIQUID_BLOCK_H)
#define LIQUID_BLOCK_H

#include "document_body.h"
#include "vm_assembler_pool.h"

typedef struct block_body {
    bool compiled;
    bool from_serialize;
    VALUE obj;
    c_buffer_t tags;

    union {
        struct {
            document_body_entry_t document_body_entry;
            VALUE nodelist;
        } compiled;
        struct {
            document_body_entry_t document_body_entry;
            VALUE parse_context;
        } serialize;
        struct {
            VALUE parse_context;
            vm_assembler_pool_t *vm_assembler_pool;
            bool blank;
            bool root;
            bool bound_to_tag;
            unsigned int render_score;
            vm_assembler_t *code;
        } intermediate;
    } as;
} block_body_t;

void liquid_define_block_body();

static inline uint8_t *block_body_instructions_ptr(block_body_header_t *body_header)
{
    return (uint8_t *)&body_header[1];
}

#endif
