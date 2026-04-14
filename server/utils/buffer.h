#ifndef UTILS_BUFFER_H
#define UTILS_BUFFER_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Buffer;

void buffer_init(Buffer *buffer, size_t initial_size);
void buffer_append(Buffer *buffer, const char *data, size_t len);
void buffer_free(Buffer *buffer);

#endif
