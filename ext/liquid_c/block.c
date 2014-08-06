#include "block.h"
#include "parse.h"
#include <string.h>

VALUE cLiquidBlockParser;
static VALUE cLiquidSyntaxError;
static VALUE mLiquidTemplate;

static void block_parser_mark(void *ptr)
{
    block_parser_t *parser = ptr;
    rb_gc_mark(parser->tokens);
    rb_gc_mark(parser->blank);
    rb_gc_mark(parser->nodelist);
    rb_gc_mark(parser->children);
    rb_gc_mark(parser->options);
    rb_gc_mark(parser->iBlock);
}

static void block_parser_free(void *ptr)
{
    block_parser_t *parser = ptr;
    xfree(parser);
}

static size_t block_parser_memsize(const void *ptr)
{
    return ptr ? sizeof(block_parser_t) : 0;
}

const rb_data_type_t block_parser_data_type = {
    "liquid_block_parser",
    { block_parser_mark, block_parser_free, block_parser_memsize, },
#if defined(RUBY_TYPED_FREE_IMMEDIATELY)
    NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
#endif
};

static VALUE block_parser_allocate(VALUE klass)
{
    VALUE obj;
    block_parser_t *block_parser;
    obj = TypedData_Make_Struct(klass, block_parser_t, &block_parser_data_type, block_parser);
    block_parser->tokens = Qnil;
    block_parser->options = Qnil;
    block_parser->iBlock = Qnil;
    block_parser->blank = Qnil;
    block_parser->nodelist = Qnil;
    block_parser->children = Qnil;
    return obj;
}

static void parse(block_parser_t *parser)
{
    for (;;) {
        VALUE token = rb_funcall(parser->tokens, rb_intern("shift"), 0);
        if (token == Qnil) break;
        const unsigned char *token_begin = (const unsigned char *)RSTRING_PTR(token);
        size_t token_len = RSTRING_LEN(token);
        if (!token_len) {
            continue;
        }
        if (token_len >= 2 && token_begin[0] == '{' && token_begin[1] == '%') {
            const unsigned char *token_end = token_begin + token_len;
            if (token_len < 4 || token_end[-2] != '%' || token_end[-1] != '}') {
                // TODO: Use locale
                VALUE err = rb_class_new_instance(0, NULL, cLiquidSyntaxError);
                rb_raise(err, " ");
                return;
            }
            const unsigned char *word_begin = skip_white(token_begin + 2, token_end);
            const unsigned char *word_end = scan_word(word_begin, token_end);

            // Check if end tag
            VALUE delimiter = rb_funcall(parser->iBlock, rb_intern("block_delimiter"), 0);
            if (word_end - word_begin == RSTRING_LEN(delimiter) &&
                !strncmp(RSTRING_PTR(delimiter), (const char *)word_begin, word_end - word_begin)) {
                rb_funcall(parser->iBlock, rb_intern("end_tag"), 0);
                return;
            }

            const unsigned char *rem_begin = skip_white(word_end, token_end);

            VALUE key = rb_enc_str_new((const char *)word_begin, word_end - word_begin, utf8_encoding);
            VALUE rem = rb_enc_str_new((const char *)rem_begin, token_end - rem_begin - 2, utf8_encoding);

            // Fetch from registered blocks
            VALUE tags = rb_funcall(mLiquidTemplate, rb_intern("tags"), 0);
            VALUE tag = rb_funcall(tags, rb_intern("[]"), 1, key);
            if (tag == Qnil) {
                rb_funcall(parser->iBlock, rb_intern("unknown_tag"), 3, key, rem, parser->tokens);
            } else {
                VALUE new_tag = rb_funcall(tag, rb_intern("parse"), 4, key, rem, parser->tokens, parser->options);
                parser->blank = parser->blank && rb_funcall(new_tag, rb_intern("blank?"), 0) == Qtrue;
                rb_funcall(parser->nodelist, rb_intern("<<"), 1, new_tag);
                rb_funcall(parser->children, rb_intern("<<"), 1, new_tag);
            }
        } else if (token_len >= 2 && token_begin[0] == '{' && token_begin[1] == '{') {
            VALUE val = rb_funcall(parser->iBlock, rb_intern("create_variable"), 1, token);
            rb_funcall(parser->nodelist, rb_intern("<<"), 1, val);
            rb_funcall(parser->children, rb_intern("<<"), 1, val);
            parser->blank = Qfalse;
        } else {
            rb_funcall(parser->nodelist, rb_intern("<<"), 1, token);
            parser->blank = all_blank(token_begin, token_begin + token_len) ? Qtrue : Qfalse;
        }
    }
    
    rb_funcall(parser->iBlock, rb_intern("assert_missing_delimitation!"), 0);
}

static VALUE block_parser_initialize_method(VALUE self,
        VALUE blank, VALUE nodelist, VALUE children,
        VALUE tokens, VALUE options, VALUE block)
{
    block_parser_t *parser;
    Block_Parser_Get_Struct(self, parser);
    parser->blank = blank;
    parser->nodelist = nodelist;
    parser->children = children;
    parser->tokens = tokens;
    parser->options = options;
    parser->iBlock = block;
    parse(parser);
    return Qnil;
}

void init_liquid_block(void)
{
    cLiquidBlockParser = rb_define_class_under(mLiquid, "BlockParser", rb_cObject);
    mLiquidTemplate = rb_path2class("Liquid::Template");
    cLiquidSyntaxError = rb_path2class("Liquid::SyntaxError");
    rb_define_alloc_func(cLiquidBlockParser, block_parser_allocate);
    rb_define_method(cLiquidBlockParser, "initialize", block_parser_initialize_method, 6);
}
