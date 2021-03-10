#include "parse_context.h"
#include "document_body.h"

static ID id_document_body, id_vm_assembler_pool, id_parent_tag;

bool parse_context_document_body_initialized_p(VALUE self)
{
    return RTEST(rb_attr_get(self, id_document_body));
}

void parse_context_init_document_body(VALUE self)
{
    assert(!parse_context_document_body_initialized_p(self));

    VALUE document_body = document_body_new_instance();
    rb_ivar_set(self, id_document_body, document_body);
}

VALUE parse_context_get_document_body(VALUE self)
{
    assert(parse_context_document_body_initialized_p(self));

    return rb_ivar_get(self, id_document_body);
}

void parse_context_remove_document_body(VALUE self)
{
    assert(parse_context_document_body_initialized_p(self));

    rb_ivar_set(self, id_document_body, Qnil);
}


vm_assembler_pool_t *parse_context_init_vm_assembler_pool(VALUE self)
{
    assert(!RTEST(rb_attr_get(self, id_vm_assembler_pool)));

    VALUE vm_assembler_pool_obj = vm_assembler_pool_new();
    rb_ivar_set(self, id_vm_assembler_pool, vm_assembler_pool_obj);

    vm_assembler_pool_t *vm_assembler_pool;
    VMAssemblerPool_Get_Struct(vm_assembler_pool_obj, vm_assembler_pool);

    return vm_assembler_pool;
}

vm_assembler_pool_t *parse_context_get_vm_assembler_pool(VALUE self)
{
    assert(RTEST(rb_attr_get(self, id_vm_assembler_pool)));

    VALUE obj = rb_ivar_get(self, id_vm_assembler_pool);
    vm_assembler_pool_t *vm_assembler_pool;
    VMAssemblerPool_Get_Struct(obj, vm_assembler_pool);
    return vm_assembler_pool;
}

void parse_context_remove_vm_assembler_pool(VALUE self)
{
    assert(RTEST(rb_attr_get(self, id_vm_assembler_pool)));

    rb_ivar_set(self, id_vm_assembler_pool, Qnil);
}


VALUE parse_context_get_parent_tag(VALUE self)
{
    return rb_attr_get(self, id_parent_tag);
}

void parse_context_set_parent_tag(VALUE self, VALUE tag_header)
{
    rb_ivar_set(self, id_parent_tag, tag_header);
}

void liquid_define_parse_context()
{
    id_document_body = rb_intern("document_body");
    id_vm_assembler_pool = rb_intern("vm_assembler_pool");
    id_parent_tag = rb_intern("parent_tag");
}
