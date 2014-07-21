#include "parse.h"

/*
 * Character type lookup table
 * 1: Whitespace (Characters that would be matched by \s in regexp's)
 * 2: Word characters (Characters under \w)
 * 3: Characters that, along with group 1, would terminate a quoted fragment
 * 4: Quote delimiters
 */
const unsigned char char_lookup[256] = {
    [' '] = 1, ['\n'] = 1, ['\r'] = 1, ['\t'] = 1, ['\f'] = 1,
    ['a'] = 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    ['A'] = 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    ['0'] = 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    ['_'] = 2,
    [','] = 3, ['|'] = 3,
    ['\''] = 4,
    ['\"'] = 4,
};
