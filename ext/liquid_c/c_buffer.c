#include "c_buffer.h"

static void c_buffer_expand_for_write(c_buffer_t *buffer, size_t write_size)
{
    size_t capacity = c_buffer_capacity(buffer);
    size_t size = c_buffer_size(buffer);
    size_t required_capacity = size + write_size;

    do {
        capacity *= 2;
    } while (capacity < required_capacity);
    buffer->data = xrealloc(buffer->data, capacity);
    buffer->data_end = buffer->data + size;
    buffer->capacity_end = buffer->data + capacity;
}

static void c_buffer_reserve_for_write(c_buffer_t *buffer, size_t write_size)
{
    uint8_t *write_end = buffer->data_end + write_size;
    if (write_end > buffer->capacity_end) {
        c_buffer_expand_for_write(buffer, write_size);
    }
}

void c_buffer_write(c_buffer_t *buffer, void *write_data, size_t write_size)
{
    c_buffer_reserve_for_write(buffer, write_size);
    memcpy(buffer->data_end, write_data, write_size);
    buffer->data_end += write_size;
}
