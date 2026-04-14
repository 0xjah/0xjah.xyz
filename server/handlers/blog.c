#include "blog.h"

#include "../core/config.h"
#include "../core/http.h"
#include "../utils/buffer.h"
#include "../utils/markdown.h"
#include "static.h"

#include <stdio.h>
#include <string.h>

#define BLOG_MAX_PATH 512

void blog_serve_post(int fd, const char *post_name, int htmx_request) {
    const Config *cfg = config_get();

    char path[BLOG_MAX_PATH];
    snprintf(path, sizeof(path), "%s/blog/%s.md", cfg->content_dir, post_name);

    Buffer file_buf = {0};
    if (static_read_file(path, &file_buf) < 0) {
        http_error(fd, 404, "Post not found");
        return;
    }

    char *md_content = markdown_frontmatter_skip(file_buf.data);

    Buffer html_buf = {0};
    if (markdown_to_html(md_content, &html_buf) != 0) {
        buffer_free(&file_buf);
        http_error(fd, 500, "Markdown parsing failed");
        return;
    }

    Buffer response = {0};
    buffer_init(&response, html_buf.size + 100);
    buffer_append(&response, "<article class=\"blog-post\">", 27);
    buffer_append(&response, html_buf.data, html_buf.size);
    buffer_append(&response, "</article>", 10);

    if (htmx_request) {
        http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);
    } else {
        Buffer page_buf = {0};
        if (static_read_file("public/blog.html", &page_buf) < 0) {
            http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);
        } else {
            const char *marker = "<div id=\"blog-post-content\" style=\"margin-top: 20px\"></div>";
            char *insert_point = strstr(page_buf.data, marker);

            if (insert_point) {
                Buffer full_page = {0};
                buffer_init(&full_page, page_buf.size + response.size);

                size_t prefix_len = (size_t)(insert_point - page_buf.data);
                buffer_append(&full_page, page_buf.data, prefix_len);
                buffer_append(&full_page,
                              "<div id=\"blog-post-content\" style=\"margin-top: 20px\">",
                              53);
                buffer_append(&full_page, response.data, response.size);
                buffer_append(&full_page, "</div>", 6);
                buffer_append(&full_page,
                              insert_point + strlen(marker),
                              page_buf.size - prefix_len - strlen(marker));

                http_send(fd, 200, "text/html; charset=utf-8", full_page.data, full_page.size, NULL);
                buffer_free(&full_page);
            } else {
                http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);
            }
            buffer_free(&page_buf);
        }
    }

    buffer_free(&response);
    buffer_free(&html_buf);
    buffer_free(&file_buf);
}
