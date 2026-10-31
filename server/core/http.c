#include "http.h"

#include "../handlers/static.h"

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
        {".xml", "application/xml"},
        {".txt", "text/plain; charset=utf-8"},
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

static const char *http_reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default:  return status < 500 ? "Client Error" : "Server Error";
    }
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
                        http_reason(status),
                        content_type,
                        len,
                        extra_headers ? extra_headers : "");

    write(fd, header, (size_t)hlen);
    if (body && len > 0) {
        write(fd, body, len);
    }
}

// Renders web/pages/error.html, filling {{code}} and {{message}}.
// message is always a server-side constant, never request data, so it isn't escaped.
void http_error(int fd, int status, const char *message) {
    Buffer page = {0};
    if (static_read_file("web/pages/error.html", &page) < 0) {
        http_send(fd, status, "text/plain", message, strlen(message), NULL);
        return;
    }

    char code[16];
    snprintf(code, sizeof(code), "%d", status);

    Buffer out = {0};
    buffer_init(&out, page.size + 256);
    const char *p = page.data;
    const char *t;
    while ((t = strstr(p, "{{")) != NULL) {
        buffer_append(&out, p, (size_t)(t - p));
        if (strncmp(t, "{{code}}", 8) == 0) {
            buffer_append(&out, code, strlen(code));
            p = t + 8;
        } else if (strncmp(t, "{{message}}", 11) == 0) {
            buffer_append(&out, message, strlen(message));
            p = t + 11;
        } else {
            buffer_append(&out, t, 2);
            p = t + 2;
        }
    }
    buffer_append(&out, p, strlen(p));

    http_send(fd, status, "text/html; charset=utf-8", out.data, out.size, NULL);
    buffer_free(&out);
    buffer_free(&page);
}
