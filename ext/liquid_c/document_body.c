#include <ruby.h>
#include "liquid.h"
#include "vm_assembler.h"
#include "document_body.h"

static VALUE cLiquidCDocumentBody;

typedef struct document_body {
    c_buffer_t buffer;
} document_body_t;

static void document_body_free(void *ptr)
{
    document_body_t *body = ptr;
    c_buffer_free(&body->buffer);
    xfree(body);
}

static size_t document_body_memsize(const void *ptr)
{
    return sizeof(document_body_t);
}

const rb_data_type_t document_body_data_type = {
    "liquid_document_body",
    { NULL, document_body_free, document_body_memsize },
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
};

#define DocumentBody_Get_Struct(obj, sval) TypedData_Get_Struct(obj, document_body_t, &document_body_data_type, sval)

static VALUE document_body_allocate(VALUE klass)
{
    document_body_t *body;

    VALUE obj = TypedData_Make_Struct(klass, document_body_t, &document_body_data_type, body);
    body->buffer = c_buffer_init();

    return obj;
}

VALUE document_body_new_instance()
{
    return rb_class_new_instance(0, NULL, cLiquidCDocumentBody);
}

void document_body_write_block_body(VALUE self, bool blank, int render_score, vm_assembler_t *code,
                                    c_buffer_t **buf, size_t *offset)
{
    document_body_t *body;
    DocumentBody_Get_Struct(self, body);

    *buf = &body->buffer;
    *offset = body->buffer.data_end - body->buffer.data;

    size_t instructions_bytes = c_buffer_size(&code->instructions);
    size_t constants_bytes = c_buffer_size(&code->constants);
    assert(constants_bytes % sizeof(VALUE *) == 0);

    block_body_t buf_block_body = {
        .instructions_offset = sizeof(block_body_t),
        .instructions_bytes = instructions_bytes,
        .constants_offset = sizeof(block_body_t) + instructions_bytes,
        .constants_bytes = constants_bytes,
        .blank = blank,
        .render_score = render_score,
        .max_stack_size = code->max_stack_size
    };

    c_buffer_write(&body->buffer, &buf_block_body, sizeof(block_body_t));
    c_buffer_concat(&body->buffer, &code->instructions);
    c_buffer_concat(&body->buffer, &code->constants);
}

void init_liquid_document_body()
{
    cLiquidCDocumentBody = rb_define_class_under(mLiquidC, "DocumentBody", rb_cObject);
    rb_global_variable(&cLiquidCDocumentBody);
    rb_define_alloc_func(cLiquidCDocumentBody, document_body_allocate);
}
