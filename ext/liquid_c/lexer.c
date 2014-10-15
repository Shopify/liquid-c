
#line 1 "lexer.rl"
#include "liquid.h"
#include "lexer.h"
#include "tokenizer.h"
#include <stdio.h>

typedef struct lexer_token {
    unsigned char type;
    char *val;
    unsigned int val_len;
} lexer_token_t;

static const char *symbol_names[256] = {
    [TOKEN_COMPARISON] = "comparison",
    [TOKEN_QUOTE] = "string",
    [TOKEN_NUMBER] = "number",
    [TOKEN_IDENTIFIER] = "id",
    [TOKEN_DOTDOT] = "dotdot",
    [TOKEN_EOS] = "end_of_string",
    ['|'] = "pipe",
    ['.'] = "dot",
    [':'] = "colon",
    [','] = "comma",
    ['['] = "open_square",
    [']'] = "close_square",
    ['('] = "open_round",
    [')'] = "close_round"
};

VALUE symbols[256] = {0};

VALUE get_rb_type(unsigned char type) {
    VALUE ret = symbols[type];
    if (ret) return ret;

    ret = ID2SYM(rb_intern(symbol_names[type]));
    symbols[type] = ret;
    return ret;
}

VALUE cLiquidSyntaxError;

#define PUSH_TOKEN(type) \
    { \
        VALUE rb_token = rb_ary_new(); \
        rb_ary_push(rb_token, get_rb_type(type)); \
        rb_ary_push(rb_token, rb_str_new(ts, te - ts)); \
        rb_ary_push(tokens, rb_token); \
    }


#line 73 "lexer.rl"



#line 58 "lexer.c"
static const char _liquid_lexer_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	5, 1, 6, 1, 7, 1, 8, 1, 
	9, 1, 10, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 2, 2, 3, 
	2, 2, 4
};

static const unsigned char _liquid_lexer_key_offsets[] = {
	0, 0, 2, 2, 4, 4, 6, 7, 
	34, 45, 55, 65, 76, 78, 79, 81, 
	82, 93, 104, 115, 126, 137, 148
};

static const char _liquid_lexer_trans_keys[] = {
	34, 92, 39, 92, 48, 57, 61, 32, 
	33, 34, 39, 44, 45, 46, 58, 60, 
	61, 62, 63, 91, 93, 95, 99, 124, 
	9, 13, 40, 41, 48, 57, 65, 90, 
	97, 122, 33, 45, 61, 63, 95, 48, 
	57, 65, 90, 97, 122, 33, 45, 63, 
	95, 48, 57, 65, 90, 97, 122, 33, 
	45, 63, 95, 48, 57, 65, 90, 97, 
	122, 33, 45, 46, 63, 95, 48, 57, 
	65, 90, 97, 122, 48, 57, 46, 61, 
	62, 61, 33, 45, 63, 95, 111, 48, 
	57, 65, 90, 97, 122, 33, 45, 63, 
	95, 110, 48, 57, 65, 90, 97, 122, 
	33, 45, 63, 95, 116, 48, 57, 65, 
	90, 97, 122, 33, 45, 63, 95, 97, 
	48, 57, 65, 90, 98, 122, 33, 45, 
	63, 95, 105, 48, 57, 65, 90, 97, 
	122, 33, 45, 63, 95, 110, 48, 57, 
	65, 90, 97, 122, 33, 45, 63, 95, 
	115, 48, 57, 65, 90, 97, 122, 0
};

static const char _liquid_lexer_single_lengths[] = {
	0, 2, 0, 2, 0, 0, 1, 17, 
	5, 4, 4, 5, 0, 1, 0, 1, 
	5, 5, 5, 5, 5, 5, 5
};

static const char _liquid_lexer_range_lengths[] = {
	0, 0, 0, 0, 0, 1, 0, 5, 
	3, 3, 3, 3, 1, 0, 1, 0, 
	3, 3, 3, 3, 3, 3, 3
};

static const unsigned char _liquid_lexer_index_offsets[] = {
	0, 0, 3, 4, 7, 8, 10, 12, 
	35, 44, 52, 60, 69, 71, 73, 75, 
	77, 86, 95, 104, 113, 122, 131
};

static const char _liquid_lexer_indicies[] = {
	1, 2, 0, 0, 1, 4, 3, 3, 
	6, 5, 7, 8, 9, 10, 0, 3, 
	11, 12, 13, 11, 15, 16, 17, 18, 
	11, 11, 18, 19, 11, 9, 11, 14, 
	18, 18, 8, 18, 18, 7, 18, 18, 
	18, 18, 18, 20, 18, 18, 18, 18, 
	18, 18, 18, 21, 18, 18, 18, 18, 
	14, 18, 18, 20, 18, 18, 23, 18, 
	18, 14, 18, 18, 22, 6, 22, 25, 
	24, 7, 26, 7, 26, 18, 18, 18, 
	18, 27, 18, 18, 18, 20, 18, 18, 
	18, 18, 28, 18, 18, 18, 20, 18, 
	18, 18, 18, 29, 18, 18, 18, 20, 
	18, 18, 18, 18, 30, 18, 18, 18, 
	20, 18, 18, 18, 18, 31, 18, 18, 
	18, 20, 18, 18, 18, 18, 32, 18, 
	18, 18, 20, 18, 18, 18, 18, 33, 
	18, 18, 18, 20, 0
};

static const char _liquid_lexer_trans_targs[] = {
	1, 7, 2, 3, 4, 7, 12, 7, 
	0, 7, 8, 7, 10, 13, 11, 14, 
	6, 15, 9, 16, 7, 7, 7, 5, 
	7, 7, 7, 17, 18, 19, 20, 21, 
	22, 9
};

static const char _liquid_lexer_trans_actions[] = {
	0, 11, 0, 0, 0, 25, 0, 9, 
	0, 7, 0, 15, 0, 0, 5, 0, 
	0, 0, 32, 0, 21, 27, 19, 0, 
	23, 13, 17, 0, 0, 0, 0, 0, 
	0, 29
};

static const char _liquid_lexer_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 1, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0
};

static const char _liquid_lexer_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 3, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0
};

static const unsigned char _liquid_lexer_eof_trans[] = {
	0, 0, 0, 0, 0, 6, 0, 0, 
	21, 22, 21, 23, 23, 25, 27, 27, 
	21, 21, 21, 21, 21, 21, 21
};

static const int liquid_lexer_start = 7;
static const int liquid_lexer_first_final = 7;
static const int liquid_lexer_error = 0;

static const int liquid_lexer_en_main = 7;


#line 76 "lexer.rl"

static VALUE lex(VALUE self, VALUE tokens, VALUE rb_input)
{
    fprintf(stderr, "Y");
    unsigned int len = RSTRING_LEN(rb_input);
    char *str = RSTRING_PTR(rb_input);

    int cs, act;
    char *p = str, *ts, *te;
    char *pe = str + len + 1;
    char *eof = pe;

    
#line 190 "lexer.c"
	{
	cs = liquid_lexer_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 89 "lexer.rl"
    
#line 200 "lexer.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_acts = _liquid_lexer_actions + _liquid_lexer_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
#line 1 "NONE"
	{ts = p;}
	break;
#line 221 "lexer.c"
		}
	}

	_keys = _liquid_lexer_trans_keys + _liquid_lexer_key_offsets[cs];
	_trans = _liquid_lexer_index_offsets[cs];

	_klen = _liquid_lexer_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _liquid_lexer_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _liquid_lexer_indicies[_trans];
_eof_trans:
	cs = _liquid_lexer_trans_targs[_trans];

	if ( _liquid_lexer_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _liquid_lexer_actions + _liquid_lexer_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 2:
#line 1 "NONE"
	{te = p+1;}
	break;
	case 3:
#line 66 "lexer.rl"
	{act = 2;}
	break;
	case 4:
#line 69 "lexer.rl"
	{act = 5;}
	break;
	case 5:
#line 65 "lexer.rl"
	{te = p+1;}
	break;
	case 6:
#line 66 "lexer.rl"
	{te = p+1;{ PUSH_TOKEN(TOKEN_COMPARISON); }}
	break;
	case 7:
#line 67 "lexer.rl"
	{te = p+1;{ PUSH_TOKEN(TOKEN_QUOTE); }}
	break;
	case 8:
#line 70 "lexer.rl"
	{te = p+1;{ PUSH_TOKEN(TOKEN_DOTDOT); }}
	break;
	case 9:
#line 71 "lexer.rl"
	{te = p+1;{ PUSH_TOKEN(*ts); }}
	break;
	case 10:
#line 66 "lexer.rl"
	{te = p;p--;{ PUSH_TOKEN(TOKEN_COMPARISON); }}
	break;
	case 11:
#line 68 "lexer.rl"
	{te = p;p--;{ PUSH_TOKEN(TOKEN_NUMBER); }}
	break;
	case 12:
#line 69 "lexer.rl"
	{te = p;p--;{ PUSH_TOKEN(TOKEN_IDENTIFIER); }}
	break;
	case 13:
#line 71 "lexer.rl"
	{te = p;p--;{ PUSH_TOKEN(*ts); }}
	break;
	case 14:
#line 68 "lexer.rl"
	{{p = ((te))-1;}{ PUSH_TOKEN(TOKEN_NUMBER); }}
	break;
	case 15:
#line 1 "NONE"
	{	switch( act ) {
	case 2:
	{{p = ((te))-1;} PUSH_TOKEN(TOKEN_COMPARISON); }
	break;
	case 5:
	{{p = ((te))-1;} PUSH_TOKEN(TOKEN_IDENTIFIER); }
	break;
	}
	}
	break;
#line 351 "lexer.c"
		}
	}

_again:
	_acts = _liquid_lexer_actions + _liquid_lexer_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
#line 1 "NONE"
	{ts = 0;}
	break;
#line 364 "lexer.c"
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _liquid_lexer_eof_trans[cs] > 0 ) {
		_trans = _liquid_lexer_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

#line 90 "lexer.rl"

    if (p < eof - 1) {
        rb_raise(cLiquidSyntaxError, "Unexpected character %c", *ts);
        return Qnil;
    }

    VALUE rb_eof = rb_ary_new();
    rb_ary_push(rb_eof, get_rb_type(TOKEN_EOS));
    rb_ary_push(tokens, rb_eof);

    return tokens;
}

void init_liquid_lexer(void)
{
    cLiquidSyntaxError = rb_const_get(mLiquid, rb_intern("SyntaxError"));
    rb_define_singleton_method(mLiquid, "c_lex", lex, 2);
}
