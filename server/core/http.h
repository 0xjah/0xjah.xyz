#ifndef CORE_HTTP_H
#define CORE_HTTP_H

#include <stddef.h>

const char *http_mime_type_get(const char *path);
void http_send(int fd, int status, const char *content_type, const char *body, size_t len,
               const char *extra_headers);
void http_error(int fd, int status, const char *message);

#endif
