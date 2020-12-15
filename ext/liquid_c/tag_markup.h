#ifndef LIQUID_TAG_MARKUP_H
#define LIQUID_TAG_MARKUP_H

#include "c_buffer.h"
#include "block.h"

typedef struct tag_markup {
    uint32_t flags;
    VALUE tag_name;
    VALUE markup;
    VALUE block_body_obj;
    block_body_t *block_body;
} tag_markup_t;

typedef struct tag_markup_header {
    uint32_t flags;
    uint32_t total_len;
    uint32_t tag_name_offset;
    uint32_t tag_name_len;
    uint32_t markup_offset;
    uint32_t markup_len;
    uint32_t block_body_offset;
} tag_markup_header_t;

#define TAG_FLAG_UNKNOWN (1 << 0)
#define TAG_UNKNOWN_P(tag) (tag->flags & TAG_FLAG_UNKNOWN)

extern const rb_data_type_t tag_markup_data_type;
#define TagMarkup_Get_Struct(obj, sval) TypedData_Get_Struct(obj, tag_markup_t, &tag_markup_data_type, sval)

void liquid_define_tag_markup();
VALUE tag_markup_new(VALUE tag_name, VALUE markup, bool unknown);
VALUE tag_markup_get_tag_name(VALUE self);
VALUE tag_markup_get_markup(VALUE self);
void tag_markup_set_block_body(VALUE self, VALUE block_body_obj, block_body_t *block_body);

#endif
