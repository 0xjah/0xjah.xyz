#ifndef HANDLERS_STATIC_H
#define HANDLERS_STATIC_H

#include "../utils/buffer.h"

int static_read_file(const char *path, Buffer *buffer);
void static_serve_file(int fd, const char *path);
void static_serve_file_status(int fd, const char *path, int status);

#endif
