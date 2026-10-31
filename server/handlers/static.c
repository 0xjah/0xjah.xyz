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
    Buffer file_buffer = {0};
    if (static_read_file(path, &file_buffer) < 0) {
        http_error(fd, 404, "There's nothing here. The page doesn't exist, or it moved.");
        return;
    }

    // Fonts and vendor files get a new filename whenever they change, so cache them forever.
    // Everything else (main.css, main.js, ...) keeps its name, so browsers must re-fetch it
    // or they mix a new page with old CSS/JS after a deploy.
    // shortcut: no Last-Modified/304 support, so "no-cache" re-downloads the file each time;
    // add validators if these files ever get large.
    const char *cache = NULL;
    if (strstr(path, "/assets/fonts/") || strstr(path, "/assets/vendor/")) {
        cache = "Cache-Control: public, max-age=31536000, immutable\r\n";
    } else if (strstr(path, "/assets/")) {
        cache = "Cache-Control: no-cache\r\n";
    }

    http_send(fd, 200, http_mime_type_get(path), file_buffer.data, file_buffer.size, cache);
    buffer_free(&file_buffer);
}
