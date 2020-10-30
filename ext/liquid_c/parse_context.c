#include "parse_context.h"
#include "document_body.h"

static ID id_document_body, id_vm_assembler_pool;

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

    VALUE vm_assembler_pool_obj = vm_assembler_pool_new_instance();
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


void init_liquid_parse_context()
{
    id_document_body = rb_intern("document_body");
    id_vm_assembler_pool = rb_intern("vm_assembler_pool");
}
