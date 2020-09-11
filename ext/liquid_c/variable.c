#include "liquid.h"
#include "variable.h"
#include "parser.h"
#include <stdio.h>

static ID id_rescue_strict_parse_syntax_error;

static void compile_expression(vm_assembler_t *code, VALUE expression, bool const_expression)
{
    if (const_expression)
        vm_assembler_add_push_const(code, expression);
    else
        vm_assembler_add_push_eval_expr(code, expression);
}

static int compile_each_keyword_arg(VALUE key, VALUE value, VALUE func_arg)
{
    vm_assembler_t *code = (vm_assembler_t *)func_arg;

    vm_assembler_add_push_const(code, key);

    bool is_const_expr = !rb_respond_to(value, id_evaluate);
    compile_expression(code, value, is_const_expr);

    return ST_CONTINUE;
}

static inline void parse_and_compile_expression(parser_t *p, vm_assembler_t *code)
{
    bool is_const = will_parse_constant_expression_next(p);
    compile_expression(code, parse_expression(p), is_const);
}

static VALUE try_variable_strict_parse(VALUE uncast_args)
{
    variable_parse_args_t *parse_args = (void *)uncast_args;
    parser_t p;
    init_parser(&p, parse_args->markup, parse_args->markup_end);
    vm_assembler_t *code = parse_args->code;

    if (p.cur.type == TOKEN_EOS)
        return Qnil;

    vm_assembler_add_render_variable_rescue(code, parse_args->line_number);

    parse_and_compile_expression(&p, code);

    while (parser_consume(&p, TOKEN_PIPE).type) {
        lexer_token_t filter_name_token = parser_must_consume(&p, TOKEN_IDENTIFIER);
        VALUE filter_name = token_to_rsym(filter_name_token);

        size_t arg_count = 0;
        bool const_keyword_args = true;
        VALUE keyword_args = Qnil;

        if (parser_consume(&p, TOKEN_COLON).type) {
            do {
                if (p.cur.type == TOKEN_IDENTIFIER && p.next.type == TOKEN_COLON) {
                    VALUE key = token_to_rstr(parser_consume_any(&p));
                    parser_consume_any(&p);

                    if (const_keyword_args && !will_parse_constant_expression_next(&p))
                        const_keyword_args = false;
                    if (keyword_args == Qnil)
                        keyword_args = rb_hash_new();
                    rb_hash_aset(keyword_args, key, parse_expression(&p));
                } else {
                    parse_and_compile_expression(&p, code);
                    arg_count++;
                }
            } while (parser_consume(&p, TOKEN_COMMA).type);
        }

        if (keyword_args != Qnil) {
            arg_count++;
            if (RHASH_SIZE(keyword_args) > 255) {
                rb_enc_raise(utf8_encoding, cLiquidSyntaxError, "Too many filter keyword arguments");
            }
            if (const_keyword_args) {
                vm_assembler_add_push_const(code, keyword_args);
            } else {
                rb_hash_foreach(keyword_args, compile_each_keyword_arg, (VALUE)code);
                vm_assembler_add_hash_new(code, RHASH_SIZE(keyword_args));
            }
        }
        if (arg_count > 254) {
            rb_enc_raise(utf8_encoding, cLiquidSyntaxError, "Too many filter arguments");
        }
        vm_assembler_add_filter(code, filter_name, arg_count);
    }

    vm_assembler_add_pop_write_variable(code);

    parser_must_consume(&p, TOKEN_EOS);

    return Qnil;
}

typedef struct variable_strict_parse_rescue {
    variable_parse_args_t *parse_args;
    size_t instructions_size;
    size_t constants_size;
    size_t stack_size;
} variable_strict_parse_rescue_t;

static VALUE variable_strict_parse_rescue(VALUE uncast_args, VALUE exception)
{
    variable_strict_parse_rescue_t *rescue_args = (void *)uncast_args;
    variable_parse_args_t *parse_args = rescue_args->parse_args;
    vm_assembler_t *code = parse_args->code;

    // undo partial strict parse
    code->instructions.data_end = code->instructions.data + rescue_args->instructions_size;
    code->constants.data_end = code->constants.data + rescue_args->constants_size;
    code->stack_size = rescue_args->stack_size;

    if (rb_obj_is_kind_of(exception, cLiquidSyntaxError) == Qfalse)
        rb_exc_raise(exception);

    VALUE markup_obj = rb_enc_str_new(parse_args->markup, parse_args->markup_end - parse_args->markup, utf8_encoding);
    VALUE variable_obj = rb_funcall(
        cLiquidVariable, id_rescue_strict_parse_syntax_error, 3,
        exception, markup_obj, parse_args->parse_context
    );

    vm_assembler_add_write_node(code, variable_obj);
    return Qnil;
}

void internal_variable_parse(variable_parse_args_t *parse_args)
{
    vm_assembler_t *code = parse_args->code;
    variable_strict_parse_rescue_t rescue_args = {
        .parse_args = parse_args,
        .instructions_size = c_buffer_size(&code->instructions),
        .constants_size = c_buffer_size(&code->constants),
        .stack_size = code->stack_size,
    };
    rb_rescue(try_variable_strict_parse, (VALUE)parse_args, variable_strict_parse_rescue, (VALUE)&rescue_args);
}

static VALUE rb_variable_parse(VALUE self, VALUE markup, VALUE filters)
{
    StringValue(markup);
    char *start = RSTRING_PTR(markup);

    parser_t p;
    init_parser(&p, start, start + RSTRING_LEN(markup));

    if (p.cur.type == TOKEN_EOS)
        return Qnil;

    VALUE name = parse_expression(&p);

    while (parser_consume(&p, TOKEN_PIPE).type) {
        lexer_token_t filter_name = parser_must_consume(&p, TOKEN_IDENTIFIER);

        VALUE filter_args = rb_ary_new(), keyword_args = Qnil, filter;

        if (parser_consume(&p, TOKEN_COLON).type) {
            do {
                if (p.cur.type == TOKEN_IDENTIFIER && p.next.type == TOKEN_COLON) {
                    VALUE key = token_to_rstr(parser_consume_any(&p));
                    parser_consume_any(&p);

                    if (keyword_args == Qnil) keyword_args = rb_hash_new();
                    rb_hash_aset(keyword_args, key, parse_expression(&p));
                } else {
                    rb_ary_push(filter_args, parse_expression(&p));
                }
            } while (parser_consume(&p, TOKEN_COMMA).type);
        }

        if (keyword_args == Qnil) {
            filter = rb_ary_new3(2, token_to_rstr(filter_name), filter_args);
        } else {
            filter = rb_ary_new3(3, token_to_rstr(filter_name), filter_args, keyword_args);
        }
        rb_ary_push(filters, filter);
    }

    parser_must_consume(&p, TOKEN_EOS);
    return name;
}

void init_liquid_variable(void)
{
    id_rescue_strict_parse_syntax_error = rb_intern("rescue_strict_parse_syntax_error");

    rb_define_singleton_method(cLiquidVariable, "c_strict_parse", rb_variable_parse, 2);
}

