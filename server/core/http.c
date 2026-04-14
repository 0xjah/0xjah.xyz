#include "http.h"

#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

const char *http_mime_type_get(const char *path) {
    static const struct {
        const char *ext;
        const char *mime;
    } types[] = {
        {".html", "text/html; charset=utf-8"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".pdf", "application/pdf"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".webp", "image/webp"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"},
        {".ttc", "font/collection"},
        {NULL, NULL}
    };

    const char *ext = strrchr(path, '.');
    if (ext) {
        for (int i = 0; types[i].ext; i++) {
            if (strcasecmp(ext, types[i].ext) == 0) {
                return types[i].mime;
            }
        }
    }
    return "application/octet-stream";
}

void http_send(int fd, int status, const char *content_type, const char *body, size_t len,
               const char *extra_headers) {
    char header[2048];
    int hlen = snprintf(header, sizeof(header),
                        "HTTP/1.1 %d %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %zu\r\n"
                        "Connection: keep-alive\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "%s"
                        "\r\n",
                        status,
                        status == 200 ? "OK" : (status == 404 ? "Not Found" : "Error"),
                        content_type,
                        len,
                        extra_headers ? extra_headers : "");

    write(fd, header, (size_t)hlen);
    if (body && len > 0) {
        write(fd, body, len);
    }
}

void http_error(int fd, int status, const char *message) {
    http_send(fd, status, "text/plain", message, strlen(message), NULL);
}
