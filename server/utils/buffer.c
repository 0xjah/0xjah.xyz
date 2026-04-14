#include "buffer.h"

#include <stdlib.h>
#include <string.h>

void buffer_init(Buffer *buf, size_t initial_size) {
    buf->capacity = initial_size;
    buf->size = 0;
    buf->data = malloc(initial_size);
    if (buf->data) {
        buf->data[0] = '\0';
    }
}

void buffer_append(Buffer *buf, const char *data, size_t len) {
    if (!buf || !data) {
        return;
    }

    if (buf->size + len + 1 > buf->capacity) {
        size_t new_capacity = (buf->size + len + 1) * 2;
        char *new_data = realloc(buf->data, new_capacity);
        if (!new_data) {
            return;
        }
        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
}

void buffer_free(Buffer *buf) {
    if (!buf) {
        return;
    }
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}
