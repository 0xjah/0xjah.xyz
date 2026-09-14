#include "blog.h"

#include "../core/config.h"
#include "../core/http.h"
#include "../utils/buffer.h"
#include "../utils/markdown.h"
#include "static.h"

#include <stdio.h>
#include <string.h>

#define BLOG_MAX_PATH 512
#define BLOG_TITLE_MAX 256

static void blog_extract_title(const char *raw, char *out, size_t out_size) {
    out[0] = '\0';

    const char *frontmatter_end = strstr(raw, "\n---");
    const char *marker = strstr(raw, "title: \"");
    if (!marker || (frontmatter_end && marker > frontmatter_end)) {
        return;
    }

    marker += 8;
    const char *end = strchr(marker, '"');
    if (!end) {
        return;
    }

    size_t len = (size_t)(end - marker);
    if (len >= out_size) {
        len = out_size - 1;
    }
    strncpy(out, marker, len);
    out[len] = '\0';
}

static void blog_html_escape(const char *in, char *out, size_t out_size) {
    size_t oi = 0;
    for (const char *p = in; *p && oi + 6 < out_size; p++) {
        switch (*p) {
            case '&': oi += (size_t)snprintf(out + oi, out_size - oi, "&amp;"); break;
            case '<': oi += (size_t)snprintf(out + oi, out_size - oi, "&lt;"); break;
            case '>': oi += (size_t)snprintf(out + oi, out_size - oi, "&gt;"); break;
            case '"': oi += (size_t)snprintf(out + oi, out_size - oi, "&quot;"); break;
            default: out[oi++] = *p;
        }
    }
    out[oi] = '\0';
}

static void blog_buffer_replace_once(Buffer *out, const char *content, size_t content_len,
                                      const char *needle, const char *replacement) {
    const char *pos = strstr(content, needle);
    if (!pos) {
        buffer_init(out, content_len);
        buffer_append(out, content, content_len);
        return;
    }

    size_t needle_len = strlen(needle);
    size_t replacement_len = strlen(replacement);
    size_t prefix_len = (size_t)(pos - content);

    buffer_init(out, content_len + replacement_len);
    buffer_append(out, content, prefix_len);
    buffer_append(out, replacement, replacement_len);
    buffer_append(out, pos + needle_len, content_len - prefix_len - needle_len);
}

typedef struct {
    const char *needle;
    const char *replacement;
} BlogReplacement;

static void blog_apply_replacements(Buffer *out, const char *content, size_t content_len,
                                     const BlogReplacement *replacements, size_t count) {
    Buffer current = {0};
    buffer_init(&current, content_len);
    buffer_append(&current, content, content_len);

    for (size_t i = 0; i < count; i++) {
        Buffer next = {0};
        blog_buffer_replace_once(&next, current.data, current.size,
                                  replacements[i].needle, replacements[i].replacement);
        buffer_free(&current);
        current = next;
    }

    *out = current;
}

void blog_serve_post(int fd, const char *post_name, int htmx_request) {
    const Config *config = config_get();

    char path[BLOG_MAX_PATH];
    snprintf(path, sizeof(path), "%s/blog/%s.md", config->content_dir, post_name);

    Buffer file_buffer = {0};
    if (static_read_file(path, &file_buffer) < 0) {
        http_error(fd, 404, "Post not found");
        return;
    }

    char *md_content = markdown_frontmatter_skip(file_buffer.data);

    Buffer html_buffer = {0};
    if (markdown_to_html(md_content, &html_buffer) != 0) {
        buffer_free(&file_buffer);
        http_error(fd, 500, "Markdown parsing failed");
        return;
    }

    Buffer response = {0};
    buffer_init(&response, html_buffer.size + 100);
    buffer_append(&response, "<article class=\"blog-post\">", 27);
    buffer_append(&response, html_buffer.data, html_buffer.size);
    buffer_append(&response, "</article>", 10);

    if (htmx_request) {
        http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);
    } else {
        Buffer page_buffer = {0};
        if (static_read_file("web/pages/blog.html", &page_buffer) < 0) {
            http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);
        } else {
            char raw_title[BLOG_TITLE_MAX];
            blog_extract_title(file_buffer.data, raw_title, sizeof(raw_title));

            Buffer described_buffer = {0};
            const char *meta_source = page_buffer.data;
            size_t meta_source_len = page_buffer.size;

            if (raw_title[0] != '\0') {
                char escaped_title[BLOG_TITLE_MAX];
                blog_html_escape(raw_title, escaped_title, sizeof(escaped_title));

                char title_tag[BLOG_TITLE_MAX + 32];
                snprintf(title_tag, sizeof(title_tag), "<title>%s | 0xjah.xyz</title>", escaped_title);

                char description_text[BLOG_TITLE_MAX + 64];
                snprintf(description_text, sizeof(description_text),
                         "%s &mdash; a post on 0xjah's blog.", escaped_title);

                char meta_desc[BLOG_TITLE_MAX + 96];
                snprintf(meta_desc, sizeof(meta_desc), "<meta name=\"description\" content=\"%s\">", description_text);

                char og_title[BLOG_TITLE_MAX + 48];
                snprintf(og_title, sizeof(og_title), "<meta property=\"og:title\" content=\"%s | 0xjah.xyz\">", escaped_title);

                char og_description[BLOG_TITLE_MAX + 96];
                snprintf(og_description, sizeof(og_description), "<meta property=\"og:description\" content=\"%s\">", description_text);

                char og_url[BLOG_MAX_PATH];
                snprintf(og_url, sizeof(og_url), "<meta property=\"og:url\" content=\"https://0xjah.xyz/blog?post=%s\">", post_name);

                char twitter_title[BLOG_TITLE_MAX + 48];
                snprintf(twitter_title, sizeof(twitter_title), "<meta name=\"twitter:title\" content=\"%s | 0xjah.xyz\">", escaped_title);

                char twitter_description[BLOG_TITLE_MAX + 96];
                snprintf(twitter_description, sizeof(twitter_description), "<meta name=\"twitter:description\" content=\"%s\">", description_text);

                const BlogReplacement replacements[] = {
                    {"<title>Blog | 0xjah.xyz</title>", title_tag},
                    {"<meta name=\"description\" content=\"Ahmad's blog &mdash; tech, philosophy, anime, movies, and religion, written by a self-taught programmer and pentester.\">", meta_desc},
                    {"<meta property=\"og:url\" content=\"https://0xjah.xyz/blog\">", og_url},
                    {"<meta property=\"og:title\" content=\"Blog | 0xjah.xyz\">", og_title},
                    {"<meta property=\"og:description\" content=\"Ahmad's blog &mdash; tech, philosophy, anime, movies, and religion, written by a self-taught programmer and pentester.\">", og_description},
                    {"<meta name=\"twitter:title\" content=\"Blog | 0xjah.xyz\">", twitter_title},
                    {"<meta name=\"twitter:description\" content=\"Ahmad's blog &mdash; tech, philosophy, anime, movies, and religion, written by a self-taught programmer and pentester.\">", twitter_description},
                };

                blog_apply_replacements(&described_buffer, page_buffer.data, page_buffer.size,
                                         replacements, sizeof(replacements) / sizeof(replacements[0]));

                meta_source = described_buffer.data;
                meta_source_len = described_buffer.size;
            }

            const char *marker = "<div id=\"blog-post-content\" style=\"margin-top: 20px\"></div>";
            const char *insert_point = strstr(meta_source, marker);

            if (insert_point) {
                Buffer full_page = {0};
                buffer_init(&full_page, meta_source_len + response.size);

                size_t prefix_len = (size_t)(insert_point - meta_source);
                buffer_append(&full_page, meta_source, prefix_len);
                buffer_append(&full_page,
                              "<div id=\"blog-post-content\" style=\"margin-top: 20px\">",
                              53);
                buffer_append(&full_page, response.data, response.size);
                buffer_append(&full_page, "</div>", 6);
                buffer_append(&full_page,
                              insert_point + strlen(marker),
                              meta_source_len - prefix_len - strlen(marker));

                http_send(fd, 200, "text/html; charset=utf-8", full_page.data, full_page.size, NULL);
                buffer_free(&full_page);
            } else {
                http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);
            }

            if (raw_title[0] != '\0') {
                buffer_free(&described_buffer);
            }
            buffer_free(&page_buffer);
        }
    }

    buffer_free(&response);
    buffer_free(&html_buffer);
    buffer_free(&file_buffer);
}
