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

VALUE tag_markup_new(uint32_t line_number, VALUE tag_name, VALUE markup, bool unknown)
{
    tag_markup_t *tag;
    VALUE obj = TypedData_Make_Struct(cLiquidCTagMarkup, tag_markup_t, &tag_markup_data_type, tag);

    tag->flags = 0;
    if (unknown) tag->flags |= TAG_FLAG_UNKNOWN;
    tag->line_number = line_number;
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

void liquid_define_tag_markup()
{
    cLiquidCTagMarkup = rb_define_class_under(mLiquidC, "TagMarkup", rb_cObject);
    rb_global_variable(&cLiquidCTagMarkup);
    rb_undef_alloc_func(cLiquidCTagMarkup);
}
