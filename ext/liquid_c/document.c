#include <ruby.h>
#include "liquid.h"
#include "document.h"
#include "parse_context.h"
#include "document_body.h"

static ID id_parse;
static VALUE cLiquidDocument;

VALUE document_parse(VALUE tokenizer, VALUE parse_context)
{
    return rb_funcall(cLiquidDocument, id_parse, 2, tokenizer, parse_context);
}

void liquid_define_document()
{
    id_parse = rb_intern("parse");

    cLiquidDocument = rb_const_get(mLiquid, rb_intern("Document"));
    rb_global_variable(&cLiquidDocument);
}
