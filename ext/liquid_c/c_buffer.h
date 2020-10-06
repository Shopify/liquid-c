#ifndef LIQUID_C_BUFFER_H
#define LIQUID_C_BUFFER_H

#include <ruby.h>

typedef struct c_buffer {
    uint8_t *data;
    uint8_t *data_end;
    uint8_t *capacity_end;
} c_buffer_t;

inline c_buffer_t c_buffer_init()
{
    return (c_buffer_t) { NULL, NULL, NULL };
}

inline c_buffer_t c_buffer_allocate(size_t capacity)
{
    uint8_t *data = xmalloc(capacity);
    return (c_buffer_t) { data, data, data + capacity };
}

inline void c_buffer_free(c_buffer_t *buffer)
{
    xfree(buffer->data);
}

inline size_t c_buffer_size(const c_buffer_t *buffer)
{
    return buffer->data_end - buffer->data;
}

inline size_t c_buffer_capacity(const c_buffer_t *buffer)
{
    return buffer->capacity_end - buffer->data;
}

void c_buffer_reserve_for_write(c_buffer_t *buffer, size_t write_size);
void c_buffer_write(c_buffer_t *buffer, void *data, size_t size);

inline void c_buffer_write_byte(c_buffer_t *buffer, uint8_t byte) {
    c_buffer_write(buffer, &byte, 1);
}

inline void c_buffer_write_ruby_value(c_buffer_t *buffer, VALUE value) {
    c_buffer_write(buffer, &value, sizeof(VALUE));
}

#endif
