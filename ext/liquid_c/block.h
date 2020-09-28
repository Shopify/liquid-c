#if !defined(LIQUID_BLOCK_H)
#define LIQUID_BLOCK_H

#include "document_body.h"
#include "vm_assembler_pool.h"

typedef struct block_body {
    bool compiled;
    VALUE obj;

    union {
        struct {
            document_body_entry_t document_body_entry;
            VALUE nodelist;
        } compiled;
        struct {
            VALUE parse_context;
            vm_assembler_pool_t *vm_assembler_pool;
            bool blank;
            bool root;
            unsigned int render_score;
            vm_assembler_t *code;
        } intermediate;
    } as;
} block_body_t;

extern const rb_data_type_t block_body_data_type;

#define BlockBody_Get_Struct(obj, sval) TypedData_Get_Struct(obj, block_body_t, &block_body_data_type, sval)

void liquid_define_block_body();
void block_body_ensure_intermediate(block_body_t *body);

static inline uint8_t *block_body_instructions_ptr(block_body_header_t *body)
{
    return ((uint8_t *)body) + body->instructions_offset;
}

#endif
