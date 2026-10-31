#include "static.h"

#include "../core/http.h"

#include <stdio.h>
#include <string.h>

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
    static_serve_file_status(fd, path, 200);
}

void static_serve_file_status(int fd, const char *path, int status) {
    Buffer file_buffer = {0};
    if (static_read_file(path, &file_buffer) < 0) {
        http_error(fd, 404, "404 Not Found");
        return;
    }

    // Font filenames change whenever the font does, so they can be cached forever.
    const char *cache = NULL;
    if (strstr(path, "/assets/fonts/")) {
        cache = "Cache-Control: public, max-age=31536000, immutable\r\n";
    } else if (strstr(path, "/assets/")) {
        cache = "Cache-Control: public, max-age=3600\r\n";
    }

    http_send(fd, status, http_mime_type_get(path), file_buffer.data, file_buffer.size, cache);
    buffer_free(&file_buffer);
}
