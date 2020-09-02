#include "c_buffer.h"

static void c_buffer_reserve(c_buffer_t *buffer, size_t required_capacity)
{
    if (buffer->capacity >= required_capacity)
        return;

    size_t new_capacity = buffer->capacity;
    do {
        new_capacity *= 2;
    } while (new_capacity < required_capacity);
    buffer->data = xrealloc(buffer->data, new_capacity);
    buffer->capacity = new_capacity;
}

void c_buffer_write(c_buffer_t *buffer, void *write_data, size_t write_size)
{
    c_buffer_reserve(buffer, buffer->size + write_size);
    memcpy(buffer->data + buffer->size, write_data, write_size);
    buffer->size += write_size;
}
