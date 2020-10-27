#include "parse_context.h"
#include "document_body.h"

static ID id_document_body;

bool parse_context_init_document_body(VALUE self)
{
    if (!RTEST(rb_attr_get(self, id_document_body))) {
        VALUE document_body = document_body_new_instance();
        rb_ivar_set(self, id_document_body, document_body);
        return true;
    }

    return false;
}

VALUE parse_context_get_document_body(VALUE self)
{
    assert(RTEST(rb_attr_get(self, id_document_body)));
    return rb_ivar_get(self, id_document_body);
}

void parse_context_remove_document_body(VALUE self)
{
    rb_ivar_set(self, id_document_body, Qnil);
}

void init_liquid_parse_context()
{
    id_document_body = rb_intern("document_body");
}
