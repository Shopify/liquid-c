#include <ruby.h>
#include "liquid.h"
#include "vm_assembler.h"
#include "document_body.h"

static VALUE cLiquidCDocumentBody;

typedef struct document_body {
    VALUE constants;
    c_buffer_t buffer;
} document_body_t;

static void document_body_mark(void *ptr)
{
    document_body_t *body = ptr;
    rb_gc_mark(body->constants);
}

static void document_body_free(void *ptr)
{
    document_body_t *body = ptr;
    c_buffer_free(&body->buffer);
    xfree(body);
}

static size_t document_body_memsize(const void *ptr)
{
    const document_body_t *body = ptr;
    return sizeof(document_body_t) + c_buffer_size(&body->buffer);
}

const rb_data_type_t document_body_data_type = {
    "liquid_document_body",
    { document_body_mark, document_body_free, document_body_memsize },
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
};

#define DocumentBody_Get_Struct(obj, sval) TypedData_Get_Struct(obj, document_body_t, &document_body_data_type, sval)

static VALUE document_body_allocate(VALUE klass)
{
    document_body_t *body;

    VALUE obj = TypedData_Make_Struct(klass, document_body_t, &document_body_data_type, body);
    body->constants = rb_ary_new();
    body->buffer = c_buffer_init();

    return obj;
}

VALUE document_body_new_instance()
{
    return rb_class_new_instance(0, NULL, cLiquidCDocumentBody);
}

void document_body_write_block_body(VALUE self, bool blank, int render_score, vm_assembler_t *code,
                                    c_buffer_t **buf, size_t *offset, VALUE *constants)
{
    document_body_t *body;
    DocumentBody_Get_Struct(self, body);

    *buf = &body->buffer;
    *offset = c_buffer_size(&body->buffer);
    *constants = body->constants;

    assert(c_buffer_size(&code->constants) % sizeof(VALUE *) == 0);

    block_body_header_t buf_block_body = {
        .instructions_offset = (uint32_t)sizeof(block_body_header_t),
        .instructions_bytes = (uint32_t)c_buffer_size(&code->instructions),
        .constants_offset = (uint32_t)RARRAY_LEN(body->constants),
        .constants_len = (uint32_t)c_buffer_size(&code->constants) / sizeof(VALUE *),
        .blank = blank,
        .render_score = render_score,
        .max_stack_size = code->max_stack_size
    };

    c_buffer_write(&body->buffer, &buf_block_body, sizeof(block_body_header_t));
    c_buffer_concat(&body->buffer, &code->instructions);

    rb_ary_cat(body->constants, (VALUE *)code->constants.data, buf_block_body.constants_len);
}

void init_liquid_document_body()
{
    cLiquidCDocumentBody = rb_define_class_under(mLiquidC, "DocumentBody", rb_cObject);
    rb_global_variable(&cLiquidCDocumentBody);
    rb_define_alloc_func(cLiquidCDocumentBody, document_body_allocate);
}
