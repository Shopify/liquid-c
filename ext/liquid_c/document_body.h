#ifndef LIQUID_DOCUMENT_BODY_H
#define LIQUID_DOCUMENT_BODY_H

#include "c_buffer.h"
#include "block.h"
#include "vm_assembler.h"

void init_liquid_document_body();
VALUE document_body_new_instance();
void document_body_write_block_body(VALUE self, bool blank, int render_score, vm_assembler_t *code,
                                    c_buffer_t **buf, size_t *offset, VALUE *constants);

#endif
