#include "static.h"

#include "../core/http.h"

#include <stdio.h>

int static_read_file(const char *path, Buffer *buffer) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buffer_init(buffer, (size_t)size + 1);
    if (!buffer->data) {
        fclose(fp);
        return -1;
    }

    fread(buffer->data, 1, (size_t)size, fp);
    buffer->size = (size_t)size;
    buffer->data[size] = '\0';

    fclose(fp);
    return 0;
}

void static_serve_file(int fd, const char *path) {
    Buffer file_buffer = {0};
    if (static_read_file(path, &file_buffer) < 0) {
        http_error(fd, 404, "404 Not Found");
        return;
    }

    http_send(fd, 200, http_mime_type_get(path), file_buffer.data, file_buffer.size, NULL);
    buffer_free(&file_buffer);
}
