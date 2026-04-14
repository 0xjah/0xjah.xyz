#ifndef UTILS_AWS_H
#define UTILS_AWS_H

#include <stddef.h>

void aws_signature_create(const char *method, const char *uri, const char *query,
                          const char *date, const char *datetime, const char *host,
                          const char *access_key, const char *secret_key,
                          char *auth_header, size_t auth_header_size);

#endif
