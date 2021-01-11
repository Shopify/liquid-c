#ifndef LIQUID_DOCUMENT_BODY_H
#define LIQUID_DOCUMENT_BODY_H

#include "c_buffer.h"
#include "vm_assembler.h"

typedef struct block_body_header {
    uint32_t instructions_bytes;
    uint32_t first_tag_offset;
    uint32_t constants_offset;
    uint32_t constants_len;
    uint32_t flags;
    uint32_t render_score;
    uint64_t max_stack_size;
} block_body_header_t;

#define BLOCK_BODY_HEADER_FLAG_BLANK (1 << 0)
#define BLOCK_BODY_HEADER_BLANK_P(header) (header->flags & BLOCK_BODY_HEADER_FLAG_BLANK)

#define BUFFER_OFFSET_UNDEF UINT32_MAX
#define BUFFER_OFFSET_UNDEF_P(val) (val == BUFFER_OFFSET_UNDEF)

typedef struct document_body {
    VALUE self;
    VALUE constants;
    c_buffer_t buffer;
} document_body_t;

/* Bump this version every time a backwards incompatible change has been made in the serialization headers.
 */
#define DOCUMENT_BODY_CURRENT_VERSION 0

typedef struct document_body_header {
    uint32_t version;
    uint32_t entrypoint_block_offset;
    uint32_t buffer_offset;
    uint32_t buffer_len;
    uint32_t constants_offset;
    uint32_t constants_len;
} document_body_header_t;

typedef struct document_body_entry {
    document_body_t *body;
    size_t buffer_offset;
} document_body_entry_t;

void liquid_define_document_body();
VALUE document_body_new_instance();
void document_body_write_block_body(VALUE self, bool blank, uint32_t render_score, vm_assembler_t *code, document_body_entry_t *entry);
VALUE document_body_dump(document_body_t *body, uint32_t entrypoint_block_offset);

static inline void document_body_entry_mark(document_body_entry_t *entry)
{
    rb_gc_mark(entry->body->self);
    rb_gc_mark(entry->body->constants);
}

static inline block_body_header_t *document_body_get_block_body_header_ptr(const document_body_entry_t *entry)
{
    return (block_body_header_t *)(entry->body->buffer.data + entry->buffer_offset);
}

static inline const VALUE *document_body_get_constants_ptr(const document_body_entry_t *entry)
{
    block_body_header_t *header = document_body_get_block_body_header_ptr(entry);
    return RARRAY_PTR(entry->body->constants) + header->constants_offset;
}

#endif
