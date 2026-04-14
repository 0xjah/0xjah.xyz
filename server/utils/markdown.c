#include "markdown.h"

#include <md4c-html.h>

#include <string.h>

static void markdown_render_cb(const MD_CHAR *html, MD_SIZE size, void *userdata) {
    Buffer *buf = (Buffer *)userdata;
    buffer_append(buf, html, size);
}

int markdown_to_html(const char *md_content, Buffer *html_out) {
    buffer_init(html_out, strlen(md_content) * 2);
    return md_html(md_content,
                   strlen(md_content),
                   markdown_render_cb,
                   html_out,
                   MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS | MD_FLAG_NOHTML,
                   0);
}

char *markdown_frontmatter_skip(char *content) {
    if (strncmp(content, "---", 3) != 0) {
        return content;
    }

    char *end = strstr(content + 3, "\n---");
    if (!end) {
        return content;
    }

    return end + 4;
}
