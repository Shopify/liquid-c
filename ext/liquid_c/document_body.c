#include <ruby.h>
#include <stdalign.h>
#include "liquid.h"
#include "vm_assembler.h"
#include "document_body.h"
#include "tag_markup.h"

static VALUE cLiquidCDocumentBody;

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

static VALUE document_body_allocate(VALUE klass)
{
    document_body_t *body;

    VALUE obj = TypedData_Make_Struct(klass, document_body_t, &document_body_data_type, body);
    body->self = obj;
    body->constants = rb_ary_new();
    body->buffer = c_buffer_init();

    return obj;
}

#define DocumentBody_Get_Struct(obj, sval) TypedData_Get_Struct(obj, document_body_t, &document_body_data_type, sval)

VALUE document_body_new_instance()
{
    return rb_class_new_instance(0, NULL, cLiquidCDocumentBody);
}

static void document_body_write_tag_markup(document_body_t *body, VALUE tag_markup_obj)
{
    tag_markup_t *tag_markup;
    TagMarkup_Get_Struct(tag_markup_obj, tag_markup);

    size_t tag_markup_offset = c_buffer_size(&body->buffer);
    c_buffer_extend_for_write(&body->buffer, sizeof(tag_markup_header_t));

    tag_markup_header_t header;
    header.flags = tag_markup->flags;

    uint32_t tag_name_len = (uint32_t)RSTRING_LEN(tag_markup->tag_name);
    header.tag_name_len = tag_name_len;
    header.tag_name_offset = (uint32_t)(c_buffer_size(&body->buffer) - tag_markup_offset);
    c_buffer_write(&body->buffer, RSTRING_PTR(tag_markup->tag_name), tag_name_len);

    uint32_t markup_len = (uint32_t)RSTRING_LEN(tag_markup->markup);
    header.markup_len = markup_len;
    header.markup_offset = (uint32_t)(c_buffer_size(&body->buffer) - tag_markup_offset);
    c_buffer_write(&body->buffer, RSTRING_PTR(tag_markup->markup), markup_len);

    if (tag_markup->block_body) {
        if (!tag_markup->block_body->compiled) {
            rb_raise(rb_eRuntimeError, "child %"PRIsVALUE" has not been frozen before the parent", tag_markup->block_body_obj);
        }

        header.block_body_offset = (uint32_t)tag_markup->block_body->as.compiled.document_body_entry.buffer_offset;
    } else {
        header.block_body_offset = BUFFER_OFFSET_UNDEF;
    }

    header.total_len = (uint32_t)(c_buffer_size(&body->buffer) - tag_markup_offset);

    memcpy(body->buffer.data + tag_markup_offset, &header, sizeof(tag_markup_header_t));
}

void document_body_write_block_body(VALUE self, bool blank, uint32_t render_score, vm_assembler_t *code, document_body_entry_t *entry)
{
    document_body_t *body;
    DocumentBody_Get_Struct(self, body);

    c_buffer_zero_pad_for_alignment(&body->buffer, alignof(block_body_header_t));

    entry->body = body;
    entry->buffer_offset = c_buffer_size(&body->buffer);

    size_t buf_block_body_offset = c_buffer_size(&body->buffer);
    c_buffer_extend_for_write(&body->buffer, sizeof(block_body_header_t));

    block_body_header_t buf_block_body;

    buf_block_body.flags = 0;
    if (blank) buf_block_body.flags |= BLOCK_BODY_HEADER_FLAG_BLANK;
    buf_block_body.render_score = render_score;
    buf_block_body.max_stack_size = code->max_stack_size;

    buf_block_body.instructions_offset = (uint32_t)(c_buffer_size(&body->buffer) - buf_block_body_offset);
    buf_block_body.instructions_bytes = (uint32_t)c_buffer_size(&code->instructions);
    c_buffer_concat(&body->buffer, &code->instructions);

    assert(c_buffer_size(&code->tags) % sizeof(VALUE) == 0);
    uint32_t tags_len = (uint32_t)(c_buffer_size(&code->tags) / sizeof(VALUE));
    buf_block_body.tags_offset = (uint32_t)(c_buffer_size(&body->buffer) - buf_block_body_offset);
    size_t tags_start_offset = c_buffer_size(&body->buffer);
    for (uint32_t i = 0; i < tags_len; i++) {
        document_body_write_tag_markup(body, ((VALUE *)code->tags.data)[i]);
    }
    buf_block_body.tags_bytes = (uint32_t)(c_buffer_size(&body->buffer) - tags_start_offset);

    assert(c_buffer_size(&code->constants) % sizeof(VALUE) == 0);
    uint32_t constants_len = (uint32_t)(c_buffer_size(&code->constants) / sizeof(VALUE));
    buf_block_body.constants_offset = (uint32_t)RARRAY_LEN(body->constants);
    buf_block_body.constants_len = constants_len;
    rb_ary_cat(body->constants, (VALUE *)code->constants.data, constants_len);

    memcpy(body->buffer.data + buf_block_body_offset, &buf_block_body, sizeof(block_body_header_t));
}


VALUE document_body_dump(document_body_t *body, uint32_t entrypoint_block_index)
{
    assert(BUILTIN_TYPE(body->constants) == T_ARRAY);

    uint32_t buffer_len = (uint32_t)c_buffer_size(&body->buffer);

    VALUE constants = rb_marshal_dump(body->constants, Qnil);
    uint32_t constants_len = (uint32_t)RSTRING_LEN(constants);

    VALUE str = rb_str_buf_new(sizeof(document_body_header_t) + buffer_len + constants_len);

    document_body_header_t header = {
        .entrypoint_block_index = entrypoint_block_index,
        .buffer_offset = sizeof(document_body_header_t),
        .buffer_len = buffer_len,
        .constants_offset = sizeof(document_body_header_t) + buffer_len,
        .constants_len = constants_len
    };

    rb_str_cat(str, (const char *)&header, sizeof(document_body_header_t));
    rb_str_cat(str, (const char *)body->buffer.data, buffer_len);
    rb_str_append(str, constants);

    return str;
}


void liquid_define_document_body()
{
    cLiquidCDocumentBody = rb_define_class_under(mLiquidC, "DocumentBody", rb_cObject);
    rb_global_variable(&cLiquidCDocumentBody);
    rb_define_alloc_func(cLiquidCDocumentBody, document_body_allocate);
}