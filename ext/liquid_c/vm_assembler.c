#include "liquid.h"
#include "vm_assembler.h"
#include "expression.h"

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
    size_t *const_ptr = (size_t *)code->constants.data;
    const uint8_t *ip = code->instructions.data;
    // Don't rely on a terminating OP_LEAVE instruction
    // since this could be called in the middle of parsing
    const uint8_t *end_ip = code->instructions.data_end;
    while (ip < end_ip) {
        switch (*ip++) {
            case OP_LEAVE:
            case OP_POP_WRITE:
            case OP_PUSH_NIL:
            case OP_PUSH_TRUE:
            case OP_PUSH_FALSE:
            case OP_FIND_VAR:
            case OP_LOOKUP_KEY:
            case OP_NEW_INT_RANGE:
                break;

            case OP_HASH_NEW:
            case OP_PUSH_INT8:
                ip++;
                break;

            case OP_PUSH_INT16:
                ip += 2;
                break;

            case OP_RENDER_VARIABLE_RESCUE:
                ip += 3;
                break;

            case OP_WRITE_RAW:
                const_ptr += 2;
                break;

            case OP_WRITE_NODE:
            case OP_PUSH_CONST:
            case OP_FIND_STATIC_VAR:
            case OP_LOOKUP_CONST_KEY:
            case OP_LOOKUP_COMMAND:
                rb_gc_mark(*const_ptr++);
                break;

            case OP_FILTER:
                ip++;
                rb_gc_mark(*const_ptr++);
                break;

            default:
                rb_bug("invalid opcode: %u", ip[-1]);
        }
    }
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
    vm_assembler_write_opcode(code, OP_WRITE_RAW);
    size_t slice[2] = { (size_t)string, size };
    c_buffer_write(&code->constants, &slice, sizeof(slice));
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
    if (hash_size > 255)
        rb_enc_raise(utf8_encoding, cLiquidSyntaxError, "Hash literal has too many keys");
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
