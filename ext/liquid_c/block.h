#if !defined(LIQUID_BLOCK_H)
#define LIQUID_BLOCK_H

#include "liquid.h"
#include "parse.h"

typedef struct block_parser {
    VALUE tokens, blank, nodelist, children, options, iBlock;
} block_parser_t;

extern VALUE cLiquidBlockParser;
extern const rb_data_type_t block_parser_data_type;
#define Block_Parser_Get_Struct(obj, sval) TypedData_Get_Struct(obj, \
        block_parser_t, &block_parser_data_type, sval)

void init_liquid_block(void);

#endif
