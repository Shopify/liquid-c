#if !defined(RUBY_WITH_ASSERT_H)
#define RUBY_WITH_ASSERT_H

// Workaround for ruby disabling assertions (https://bugs.ruby-lang.org/issues/18777)
#ifdef NDEBUG
#include <ruby.h>
#else
#include <ruby.h>
#undef NDEBUG
#endif
// DO NOT MOVE. If assert.h is included before ruby.h asserts will turn to no-op.
#include <assert.h>

#endif
