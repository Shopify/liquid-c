#include "liquid.h"
#include "vm_assembler.h"
#include "expression.h"
#include "vm.h"

void vm_assembler_init(vm_assembler_t *code)
{
    code->instructions = c_buffer_allocate(8);
    code->constants = c_buffer_allocate(8 * sizeof(VALUE));
    code->max_stack_size = 0;
    code->stack_size = 0;
    code->protected_stack_size = 0;
    code->parsing = true;
}

void vm_assembler_free(vm_assembler_t *code)
{
    c_buffer_free(&code->instructions);
    c_buffer_free(&code->constants);
}

void vm_assembler_gc_mark(vm_assembler_t *code)
{
    c_buffer_rb_gc_mark(&code->constants);
}

VALUE vm_assembler_disassemble(const uint8_t *start_ip, const uint8_t *end_ip, const VALUE *const_ptr)
{
    const uint8_t *ip = start_ip;
    VALUE output = rb_str_buf_new(32);
    while (ip < end_ip) {
        rb_str_catf(output, "0x%04lx: ", ip - start_ip);
        switch (*ip) {
            case OP_LEAVE:
                rb_str_catf(output, "leave\n");
                break;

            case OP_POP_WRITE:
                rb_str_catf(output, "pop_write\n");
                break;

            case OP_PUSH_NIL:
                rb_str_catf(output, "push_nil\n");
                break;

            case OP_PUSH_TRUE:
                rb_str_catf(output, "push_true\n");
                break;

            case OP_PUSH_FALSE:
                rb_str_catf(output, "push_false\n");
                break;

            case OP_FIND_VAR:
                rb_str_catf(output, "find_var\n");
                break;

            case OP_LOOKUP_KEY:
                rb_str_catf(output, "lookup_key\n");
                break;

            case OP_NEW_INT_RANGE:
                rb_str_catf(output, "new_int_range\n");
                break;

            case OP_HASH_NEW:
                rb_str_catf(output, "hash_new(%u)\n", ip[1]);
                break;

            case OP_PUSH_INT8:
                rb_str_catf(output, "push_int8(%u)\n", ip[1]);
                break;

            case OP_PUSH_INT16:
            {
                int num = (ip[1] << 8) | ip[2];
                rb_str_catf(output, "push_int16(%u)\n", num);
                break;
            }

            case OP_RENDER_VARIABLE_RESCUE:
            {
                unsigned int line_number = decode_node_line_number(ip + 1);
                rb_str_catf(output, "render_variable_rescue(line_number: %u)\n", line_number);
                break;
            }

            case OP_WRITE_RAW_W:
            case OP_WRITE_RAW:
            {
                const char *text;
                size_t size;
                const char *name;
                if (*ip == OP_WRITE_RAW_W) {
                    name = "write_raw_w";
                    size = bytes_to_uint24(&ip[1]);
                    text = (const char *)&ip[4];
                } else {
                    name = "write_raw";
                    size = ip[1];
                    text = (const char *)&ip[2];
                }
                VALUE string = rb_enc_str_new(text, size, utf8_encoding);
                rb_str_catf(output, "%s(%+"PRIsVALUE")\n", name, string);
                break;
            }

            case OP_WRITE_NODE:
                rb_str_catf(output, "write_node(%+"PRIsVALUE")\n", const_ptr[0]);
                break;

            case OP_PUSH_CONST:
                rb_str_catf(output, "push_const(%+"PRIsVALUE")\n", const_ptr[0]);
                break;

            case OP_FIND_STATIC_VAR:
                rb_str_catf(output, "find_static_var(%+"PRIsVALUE")\n", const_ptr[0]);
                break;

            case OP_LOOKUP_CONST_KEY:
                rb_str_catf(output, "lookup_const_key(%+"PRIsVALUE")\n", const_ptr[0]);
                break;

            case OP_LOOKUP_COMMAND:
                rb_str_catf(output, "lookup_command(%+"PRIsVALUE")\n", const_ptr[0]);
                break;

            case OP_FILTER:
                rb_str_catf(output, "filter(name: %+"PRIsVALUE", num_args: %u)\n", const_ptr[0], ip[1]);
                break;

            default:
                rb_str_catf(output, "<opcode number %d disassembly not implemented>\n", ip[0]);
                break;
        }
        liquid_vm_next_instruction(&ip, &const_ptr);
    }
    return output;
}

void vm_assembler_concat(vm_assembler_t *dest, vm_assembler_t *src)
{
    c_buffer_concat(&dest->instructions, &src->instructions);
    c_buffer_concat(&dest->constants, &src->constants);

    size_t max_src_stack_size = dest->stack_size + src->max_stack_size;
    if (max_src_stack_size > dest->max_stack_size)
        dest->max_stack_size = max_src_stack_size;

    dest->stack_size += src->stack_size;
}

void vm_assembler_require_stack_args(vm_assembler_t *code, unsigned int count)
{
    if (code->stack_size < code->protected_stack_size + count) {
        rb_raise(rb_eRuntimeError, "insufficient number of values on the stack");
    }
}


void vm_assembler_add_write_raw(vm_assembler_t *code, const char *string, size_t size)
{
    if (size > UINT8_MAX) {
        uint8_t *instructions = c_buffer_extend_for_write(&code->instructions, 4);
        instructions[0] = OP_WRITE_RAW_W;
        uint24_to_bytes((unsigned int)size, &instructions[1]);
    } else {
        uint8_t *instructions = c_buffer_extend_for_write(&code->instructions, 2);
        instructions[0] = OP_WRITE_RAW;
        instructions[1] = size;
    }

    c_buffer_write(&code->instructions, (char *)string, size);
}

void vm_assembler_add_write_node(vm_assembler_t *code, VALUE node)
{
    vm_assembler_write_opcode(code, OP_WRITE_NODE);
    vm_assembler_write_ruby_constant(code, node);
}

void vm_assembler_add_push_fixnum(vm_assembler_t *code, VALUE num)
{
    long x = FIX2LONG(num);
    if (x >= INT8_MIN && x <= INT8_MAX) {
        vm_assembler_add_push_int8(code, x);
    } else if (x >= INT16_MIN && x <= INT16_MAX) {
        vm_assembler_add_push_int16(code, x);
    } else {
        vm_assembler_add_push_const(code, num);
    }
}

void vm_assembler_add_push_literal(vm_assembler_t *code, VALUE literal)
{
    switch (literal) {
    case Qnil:
        vm_assembler_add_push_nil(code);
        break;
    case Qtrue:
        vm_assembler_add_push_true(code);
        break;
    case Qfalse:
        vm_assembler_add_push_false(code);
        break;
    default:
        if (RB_FIXNUM_P(literal)) {
            vm_assembler_add_push_fixnum(code, literal);
        } else {
            vm_assembler_add_push_const(code, literal);
        }
        break;
    }
}

static void ensure_parsing(vm_assembler_t *code)
{
    if (!code->parsing)
        rb_raise(rb_eRuntimeError, "cannot extend code after it has finished being compiled");
}

void vm_assembler_add_evaluate_expression_from_ruby(vm_assembler_t *code, VALUE code_obj, VALUE expression)
{
    ensure_parsing(code);

    if (RB_SPECIAL_CONST_P(expression)) {
        vm_assembler_add_push_literal(code, expression);
        return;
    }

    switch (RB_BUILTIN_TYPE(expression)) {
        case T_DATA:
            if (RBASIC_CLASS(expression) == cLiquidCExpression) {
                vm_assembler_concat(code, &((expression_t *)DATA_PTR(expression))->code);
                vm_assembler_remove_leave(code);
                return;
            }
            break;
        case T_OBJECT:
        {
            VALUE klass = RBASIC_CLASS(expression);
            if (klass == cLiquidVariableLookup || klass == cLiquidRangeLookup) {
                rb_funcall(expression, id_compile_evaluate, 1, code_obj);
                return;
            }
            break;
        }
        default:
            break;
    }
    vm_assembler_add_push_const(code, expression);
}

void vm_assembler_add_find_variable_from_ruby(vm_assembler_t *code, VALUE code_obj, VALUE expression)
{
    ensure_parsing(code);

    if (RB_TYPE_P(expression, T_STRING)) {
        vm_assembler_add_find_static_variable(code, expression);
    } else {
        vm_assembler_add_evaluate_expression_from_ruby(code, code_obj, expression);
        vm_assembler_add_find_variable(code);
    }
}

void vm_assembler_add_lookup_command_from_ruby(vm_assembler_t *code, VALUE command)
{
    StringValue(command);
    ensure_parsing(code);
    vm_assembler_require_stack_args(code, 1);

    vm_assembler_add_lookup_command(code, command);
}

void vm_assembler_add_lookup_key_from_ruby(vm_assembler_t *code, VALUE code_obj, VALUE expression)
{
    ensure_parsing(code);
    vm_assembler_require_stack_args(code, 1);

    if (RB_TYPE_P(expression, T_STRING)) {
        vm_assembler_add_lookup_const_key(code, expression);
    } else {
        vm_assembler_add_evaluate_expression_from_ruby(code, code_obj, expression);
        vm_assembler_add_lookup_key(code);
    }
}

void vm_assembler_add_new_int_range_from_ruby(vm_assembler_t *code)
{
    ensure_parsing(code);
    vm_assembler_require_stack_args(code, 2);
    vm_assembler_add_new_int_range(code);
}

void vm_assembler_add_hash_new_from_ruby(vm_assembler_t *code, VALUE hash_size_obj)
{
    ensure_parsing(code);
    unsigned int hash_size = NUM2USHORT(hash_size_obj);
    vm_assembler_require_stack_args(code, hash_size * 2);

    vm_assembler_add_hash_new(code, hash_size);
}

void vm_assembler_add_filter_from_ruby(vm_assembler_t *code, VALUE filter_name, VALUE arg_count_obj)
{
    ensure_parsing(code);
    unsigned int arg_count = NUM2USHORT(arg_count_obj);
    vm_assembler_require_stack_args(code, arg_count + 1);
    filter_name = rb_str_intern(filter_name);

    vm_assembler_add_filter(code, filter_name, arg_count);
}
