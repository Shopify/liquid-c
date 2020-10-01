#include "liquid.h"
#include "resource_limits.h"

VALUE cLiquidResourceLimits;
static VALUE id_plus, id_minus, id_gt, id_bytesize;

static void resource_limits_mark(void *ptr)
{
    resource_limits_t *resource_limits = ptr;
    rb_gc_mark(resource_limits->render_length_limit);
    rb_gc_mark(resource_limits->render_score_limit);
    rb_gc_mark(resource_limits->assign_score_limit);
    rb_gc_mark(resource_limits->last_capture_length);
    rb_gc_mark(resource_limits->render_score);
    rb_gc_mark(resource_limits->assign_score);
}

static void resource_limits_free(void *ptr)
{
    resource_limits_t *resource_limits = ptr;
    xfree(resource_limits);
}

static size_t resource_limits_memsize(const void *ptr)
{
    return ptr ? sizeof(resource_limits_t) : 0;
}

const rb_data_type_t resource_limits_data_type = {
    "liquid_resource_limits",
    { resource_limits_mark, resource_limits_free, resource_limits_memsize },
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE resource_limits_allocate(VALUE klass)
{
    resource_limits_t *resource_limits;

    VALUE obj = TypedData_Make_Struct(klass, resource_limits_t, &resource_limits_data_type, resource_limits);

    return obj;
}

static void resource_limits_reset(resource_limits_t *resource_limit)
{
    resource_limit->reached_limit = 0;
    resource_limit->last_capture_length = Qnil;
    resource_limit->render_score = INT2FIX(0);
    resource_limit->assign_score = INT2FIX(0);
}

static VALUE resource_limits_initialize_method(VALUE self, VALUE limits)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    resource_limits->render_length_limit = rb_hash_lookup(limits, ID2SYM(rb_intern("render_length_limit")));
    resource_limits->render_score_limit = rb_hash_lookup(limits, ID2SYM(rb_intern("render_score_limit")));
    resource_limits->assign_score_limit = rb_hash_lookup(limits, ID2SYM(rb_intern("assign_score_limit")));

    resource_limits_reset(resource_limits);

    return Qnil;
}

static VALUE resource_limits_render_length_limit_method(VALUE self)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    return resource_limits->render_length_limit;
}

static VALUE resource_limits_set_render_length_limit_method(VALUE self, VALUE render_length_limit)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    resource_limits->render_length_limit = render_length_limit;

    return Qnil;
}

static VALUE resource_limits_render_score_limit_method(VALUE self)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    return resource_limits->render_score_limit;
}

static VALUE resource_limits_set_render_score_limit_method(VALUE self, VALUE render_score_limit)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    resource_limits->render_score_limit = render_score_limit;

    return Qnil;
}

static VALUE resource_limits_assign_score_limit_method(VALUE self)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    return resource_limits->assign_score_limit;
}

static VALUE resource_limits_set_assign_score_limit_method(VALUE self, VALUE assign_score_limit)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    resource_limits->assign_score_limit = assign_score_limit;

    return Qnil;
}

static VALUE resource_limits_render_score_method(VALUE self)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    return resource_limits->render_score;
}

static VALUE resource_limits_assign_score_method(VALUE self)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    return resource_limits->assign_score;
}

__attribute__((noreturn))
static void resource_limits_raise_limits_reached(resource_limits_t *resource_limit)
{
    resource_limit->reached_limit = 1;
    rb_raise(cMemoryError, "Memory limits exceeded");
}

static VALUE resource_limits_increment_render_score_method(VALUE self, VALUE amount)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    resource_limits->render_score = rb_funcall(resource_limits->render_score, id_plus, 1, amount);

    if (resource_limits->render_score_limit != Qnil &&
        rb_funcall(resource_limits->render_score, rb_intern(">"), 1, resource_limits->render_score_limit) == Qtrue) {
        resource_limits_raise_limits_reached(resource_limits);
    }

    return Qnil;
}

static void resource_limits_increment_assign_score(resource_limits_t *resource_limits, VALUE amount)
{
    resource_limits->assign_score = rb_funcall(resource_limits->assign_score, id_plus, 1, amount);

    if (resource_limits->assign_score_limit != Qnil &&
        rb_funcall(resource_limits->assign_score, id_gt, 1, resource_limits->assign_score_limit) == Qtrue) {
        resource_limits_raise_limits_reached(resource_limits);
    }
}

static VALUE resource_limits_increment_assign_score_method(VALUE self, VALUE amount)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    resource_limits_increment_assign_score(resource_limits, amount);

    return Qnil;
}

static VALUE resource_limits_increment_write_score_method(VALUE self, VALUE output)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    if (resource_limits->last_capture_length != Qnil) {
        VALUE captured = rb_funcall(output, id_bytesize, 0);
        VALUE increment = rb_funcall(captured, id_minus, 1, resource_limits->last_capture_length);
        resource_limits->last_capture_length = captured;
        resource_limits_increment_assign_score(resource_limits, increment);
    } else if (resource_limits->render_length_limit != Qnil) {
        VALUE captured = rb_funcall(output, id_bytesize, 0);
        if (rb_funcall(captured, id_gt, 1, resource_limits->render_length_limit)) {
            resource_limits_raise_limits_reached(resource_limits);
        }
    }

    return Qnil;
}

static VALUE resource_limits_raise_limits_reached_method(VALUE self)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    resource_limits_raise_limits_reached(resource_limits);
}

static VALUE resource_limits_reached_method(VALUE self)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    return resource_limits->reached_limit;
}

static VALUE yield_capture(VALUE _data)
{
    return rb_yield(Qundef);
}

struct capture_ensure_t {
    resource_limits_t *resource_limits;
    VALUE old_capture_length;
};

static VALUE capture_ensure(VALUE data)
{
    struct capture_ensure_t *ensure_data = (struct capture_ensure_t *)data;
    ensure_data->resource_limits->last_capture_length = ensure_data->old_capture_length;

    return Qnil;
}

static VALUE resource_limits_with_capture_method(VALUE self)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);

    struct capture_ensure_t ensure_data = {
        .resource_limits = resource_limits,
        .old_capture_length = resource_limits->last_capture_length
    };

    resource_limits->last_capture_length = INT2FIX(0);

    return rb_ensure(yield_capture, 0, capture_ensure, (VALUE)&ensure_data);
}


static VALUE resource_limits_reset_method(VALUE self)
{
    resource_limits_t *resource_limits;
    ResourceLimits_Get_Struct(self, resource_limits);
    resource_limits_reset(resource_limits);
    return Qnil;
}

void init_liquid_resource_limits()
{
    id_plus = rb_intern("+");
    id_minus = rb_intern("-");
    id_gt = rb_intern(">");
    id_bytesize = rb_intern("bytesize");

    cLiquidResourceLimits = rb_define_class_under(mLiquidC, "ResourceLimits", rb_cObject);
    rb_global_variable(&cLiquidResourceLimits);

    rb_define_alloc_func(cLiquidResourceLimits, resource_limits_allocate);
    rb_define_method(cLiquidResourceLimits, "initialize", resource_limits_initialize_method, 1);
    rb_define_method(cLiquidResourceLimits, "render_length_limit", resource_limits_render_length_limit_method, 0);
    rb_define_method(cLiquidResourceLimits, "render_length_limit=", resource_limits_set_render_length_limit_method, 1);
    rb_define_method(cLiquidResourceLimits, "render_score_limit", resource_limits_render_score_limit_method, 0);
    rb_define_method(cLiquidResourceLimits, "render_score_limit=", resource_limits_set_render_score_limit_method, 1);
    rb_define_method(cLiquidResourceLimits, "assign_score_limit", resource_limits_assign_score_limit_method, 0);
    rb_define_method(cLiquidResourceLimits, "assign_score_limit=", resource_limits_set_assign_score_limit_method, 1);
    rb_define_method(cLiquidResourceLimits, "render_score", resource_limits_render_score_method, 0);
    rb_define_method(cLiquidResourceLimits, "assign_score", resource_limits_assign_score_method, 0);
    rb_define_method(cLiquidResourceLimits, "increment_render_score", resource_limits_increment_render_score_method, 1);
    rb_define_method(cLiquidResourceLimits, "increment_assign_score", resource_limits_increment_assign_score_method, 1);
    rb_define_method(cLiquidResourceLimits, "increment_write_score", resource_limits_increment_write_score_method, 1);
    rb_define_method(cLiquidResourceLimits, "raise_limits_reached", resource_limits_raise_limits_reached_method, 0);
    rb_define_method(cLiquidResourceLimits, "reached?", resource_limits_reached_method, 0);
    rb_define_method(cLiquidResourceLimits, "reset", resource_limits_reset_method, 0);
    rb_define_method(cLiquidResourceLimits, "with_capture", resource_limits_with_capture_method, 0);
}
