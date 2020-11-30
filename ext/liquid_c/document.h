#ifndef LIQUID_DOCUMENT_H
#define LIQUID_DOCUMENT_H

void liquid_define_document();
VALUE document_parse(VALUE tokenizer, VALUE parse_context);

#endif
