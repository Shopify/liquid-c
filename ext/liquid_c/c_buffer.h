#ifndef LIQUID_C_BUFFER_H
#define LIQUID_C_BUFFER_H

#include <ruby.h>

typedef struct c_buffer {
    uint8_t *data;
    size_t size;
    size_t capacity;
} c_buffer_t;

inline c_buffer_t c_buffer_allocate(size_t capacity)
{
    return (c_buffer_t) { xmalloc(capacity), 0, capacity };
}

inline void c_buffer_free(c_buffer_t *buffer)
{
    xfree(buffer->data);
}

void c_buffer_write(c_buffer_t *buffer, void *data, size_t size);

inline void c_buffer_write_byte(c_buffer_t *buffer, uint8_t byte) {
    c_buffer_write(buffer, &byte, 1);
}

inline void c_buffer_write_ruby_value(c_buffer_t *buffer, VALUE value) {
    c_buffer_write(buffer, &value, sizeof(VALUE));
}

#endif
