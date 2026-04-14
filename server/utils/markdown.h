#ifndef UTILS_MARKDOWN_H
#define UTILS_MARKDOWN_H

#include "buffer.h"

int markdown_to_html(const char *md_content, Buffer *html_out);
char *markdown_frontmatter_skip(char *content);

#endif
