#include <ruby.h>
#include "serialize_parse_context.h"
#include "liquid.h"
#include "parse_context.h"

static VALUE cLiquidCSerializeParseContext;
static ID id_initialize;

static void serialize_parse_context_mark(void *ptr)
{
    serialize_parse_context_t *serialize_context = ptr;
    rb_gc_mark(serialize_context->document_body);
}

static void serialize_parse_context_free(void *ptr)
{
    xfree(ptr);
}

static size_t serialize_parse_context_memsize(const void *ptr)
{
    return sizeof(serialize_parse_context_t);
}

const rb_data_type_t serialize_parse_context_data_type = {
    "liquid_serialize_parse_context",
    { serialize_parse_context_mark, serialize_parse_context_free, serialize_parse_context_memsize, },
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
};

VALUE serialize_parse_context_new(VALUE document_body, document_body_header_t *header, VALUE options)
{
    VALUE obj;
    serialize_parse_context_t *serialize_context;

    obj = TypedData_Make_Struct(cLiquidCSerializeParseContext, serialize_parse_context_t,
                                &serialize_parse_context_data_type, serialize_context);
    assert(header->entrypoint_block_offset < header->buffer_len);
    serialize_context->document_body = document_body;
    document_body_setup_entry_for_header(document_body, header->entrypoint_block_offset,
                                         &serialize_context->current_entry);

    // Call initialize method of parent class
    rb_funcall(obj, id_initialize, 1, options);

    return obj;
}

bool is_serialize_parse_context_p(VALUE self)
{
    return CLASS_OF(self) == cLiquidCSerializeParseContext;
}

void serialize_parse_context_enter_tag(serialize_parse_context_t *serialize_context, tag_markup_header_t *tag)
{
    serialize_context->current_entry.buffer_offset = tag->block_body_offset;
}

void serialize_parse_context_exit_tag(serialize_parse_context_t *serialize_context, document_body_entry_t *entry,
                                      tag_markup_header_t *tag)
{
    assert(serialize_context->current_entry.body == entry->body);
    serialize_context->current_entry = *entry;
}

void liquid_define_serialize_parse_context()
{
    id_initialize = rb_intern("initialize");

    cLiquidCSerializeParseContext = rb_define_class_under(mLiquidC, "SerializeParseContext", cLiquidParseContext);
    rb_global_variable(&cLiquidCSerializeParseContext);
    rb_undef_alloc_func(cLiquidCSerializeParseContext);
}
