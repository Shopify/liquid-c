#include "liquid.h"
#include "tag_markup.h"

static VALUE cLiquidCTagMarkup;

static void tag_markup_mark(void *ptr)
{
    tag_markup_t *markup = ptr;

    rb_gc_mark(markup->markup);
    rb_gc_mark(markup->tag_name);
    rb_gc_mark(markup->block_body_obj);
}

static void tag_markup_free(void *ptr)
{
    xfree(ptr);
}

static size_t tag_markup_memsize(const void *ptr)
{
    return sizeof(tag_markup_t);
}

const rb_data_type_t tag_markup_data_type = {
    "liquid_tag_markup",
    { tag_markup_mark, tag_markup_free, tag_markup_memsize, },
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
};

VALUE tag_markup_new(VALUE tag_name, VALUE markup, bool unknown)
{
    tag_markup_t *tag;
    VALUE obj = TypedData_Make_Struct(cLiquidCTagMarkup, tag_markup_t, &tag_markup_data_type, tag);

    tag->flags = 0;
    if (unknown) tag->flags |= TAG_FLAG_UNKNOWN;
    tag->tag_name = tag_name;
    tag->markup = markup;
    tag->block_body = NULL;

    return obj;
}

VALUE tag_markup_get_tag_name(VALUE self)
{
    tag_markup_t *tag;
    TagMarkup_Get_Struct(self, tag);
    return tag->tag_name;
}

VALUE tag_markup_get_markup(VALUE self)
{
    tag_markup_t *tag;
    TagMarkup_Get_Struct(self, tag);
    return tag->markup;
}

void tag_markup_set_block_body(VALUE self, VALUE block_body_obj, block_body_t *block_body)
{
    tag_markup_t *tag;
    TagMarkup_Get_Struct(self, tag);
    assert(tag->block_body == NULL);
    tag->block_body_obj = block_body_obj;
    tag->block_body = block_body;
}

tag_markup_header_t *tag_markup_get_next_tag(document_body_entry_t *entry, tag_markup_header_t *current_tag)
{
    // Should only be used for (deserialized) immutable document body
    assert(!entry->body->mutable);

    if (BUFFER_OFFSET_UNDEF_P(entry->buffer_offset)) {
        return NULL;
    }

    block_body_header_t *header = document_body_get_block_body_header_ptr(entry);

    tag_markup_header_t *next_tag;
    if (current_tag) {
        assert(current_tag >= (tag_markup_header_t *)((char *)header + header->tags_offset));
        next_tag = (tag_markup_header_t *)((char *)current_tag + current_tag->total_len);
    } else {
        next_tag = (tag_markup_header_t *)((char *)header + header->tags_offset);
    }

    tag_markup_header_t *tags_end = (tag_markup_header_t *)((char *)header + header->tags_offset + header->tags_bytes);

    if (next_tag < tags_end) {
        assert((unsigned long)tags_end - (unsigned long)next_tag > sizeof(tag_markup_header_t));
        return next_tag;
    } else { // End of tags have been reached
        assert(next_tag == tags_end);
        return NULL;
    }
}

void liquid_define_tag_markup()
{
    cLiquidCTagMarkup = rb_define_class_under(mLiquidC, "TagMarkup", rb_cObject);
    rb_global_variable(&cLiquidCTagMarkup);
    rb_undef_alloc_func(cLiquidCTagMarkup);
}
