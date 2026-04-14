#ifndef HANDLERS_STATIC_H
#define HANDLERS_STATIC_H

#include "../utils/buffer.h"

int static_read_file(const char *path, Buffer *buf);
void static_serve_file(int fd, const char *path);

#endif
