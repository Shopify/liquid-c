#ifndef LIQUID_SERIALIZE_PARSE_CONTEXT_H
#define LIQUID_SERIALIZE_PARSE_CONTEXT_H

#include "document_body.h"
#include "tag_markup.h"

typedef struct serialize_parse_context {
    bool deserialize_complete;
    VALUE document_body;
    document_body_entry_t current_entry;
    tag_markup_header_t *current_tag;
} serialize_parse_context_t;

extern const rb_data_type_t serialize_parse_context_data_type;
#define SerializeParseContext_Get_Struct(obj, sval) TypedData_Get_Struct(obj, serialize_parse_context_t, &serialize_parse_context_data_type, sval)

void liquid_define_serialize_parse_context();
VALUE serialize_parse_context_new(VALUE document_body, document_body_header_t *header, VALUE options,
                                  serialize_parse_context_t **serialize_context_ptr_ptr);
bool is_parse_context_for_serialize(VALUE self);
void serialize_parse_context_enter_tag(serialize_parse_context_t *serialize_context, tag_markup_header_t *tag);
void serialize_parse_context_exit_tag(serialize_parse_context_t *serialize_context,  document_body_entry_t *entry, tag_markup_header_t *tag);

#endif
