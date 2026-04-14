#include "static.h"

#include "../core/http.h"

#include <stdio.h>

int static_read_file(const char *path, Buffer *buf) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buffer_init(buf, (size_t)size + 1);
    if (!buf->data) {
        fclose(fp);
        return -1;
    }

    fread(buf->data, 1, (size_t)size, fp);
    buf->size = (size_t)size;
    buf->data[size] = '\0';

    fclose(fp);
    return 0;
}

void static_serve_file(int fd, const char *path) {
    Buffer buf = {0};
    if (static_read_file(path, &buf) < 0) {
        http_error(fd, 404, "404 Not Found");
        return;
    }

    http_send(fd, 200, http_mime_type_get(path), buf.data, buf.size, NULL);
    buffer_free(&buf);
}
