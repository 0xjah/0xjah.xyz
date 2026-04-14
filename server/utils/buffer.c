#include "buffer.h"

#include <stdlib.h>
#include <string.h>

void buffer_init(Buffer *buffer, size_t initial_size) {
    buffer->capacity = initial_size;
    buffer->size = 0;
    buffer->data = malloc(initial_size);
    if (buffer->data) {
        buffer->data[0] = '\0';
    }
}

void buffer_append(Buffer *buffer, const char *data, size_t len) {
    if (!buffer || !data) {
        return;
    }

    if (buffer->size + len + 1 > buffer->capacity) {
        size_t new_capacity = (buffer->size + len + 1) * 2;
        char *new_data = realloc(buffer->data, new_capacity);
        if (!new_data) {
            return;
        }
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }

    memcpy(buffer->data + buffer->size, data, len);
    buffer->size += len;
    buffer->data[buffer->size] = '\0';
}

void buffer_free(Buffer *buffer) {
    if (!buffer) {
        return;
    }
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
}
