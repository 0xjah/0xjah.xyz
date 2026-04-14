#include "router.h"

#include "http.h"
#include "../handlers/blog.h"
#include "../handlers/gallery.h"
#include "../handlers/github.h"
#include "../handlers/resume.h"
#include "../handlers/static.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define ROUTER_BUFFER_SIZE 16384
#define ROUTER_MAX_PATH 512

static void router_query_param_get(const char *query, const char *param, char *value, size_t size) {
    value[0] = '\0';
    if (!query || !param) {
        return;
    }

    size_t param_len = strlen(param);
    const char *p = query;

    while ((p = strstr(p, param)) != NULL) {
        if ((p == query || *(p - 1) == '&') && *(p + param_len) == '=') {
            p += param_len + 1;
            const char *end = strchr(p, '&');
            size_t len = end ? (size_t)(end - p) : strlen(p);
            if (len >= size) {
                len = size - 1;
            }
            strncpy(value, p, len);
            value[len] = '\0';
            return;
        }
        p++;
    }
}

static int router_strcasestr_exists(const char *haystack, const char *needle) {
    if (!haystack || !needle) {
        return 0;
    }

    size_t needle_len = strlen(needle);
    for (const char *p = haystack; *p; p++) {
        if (strncasecmp(p, needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static int router_is_htmx_request(const char *buffer) {
    if (router_strcasestr_exists(buffer, "HX-Boosted: true") ||
        router_strcasestr_exists(buffer, "HX-Boosted:true")) {
        return 0;
    }

    return router_strcasestr_exists(buffer, "HX-Request: true") ||
           router_strcasestr_exists(buffer, "HX-Request:true") ||
           router_strcasestr_exists(buffer, "hx-request: true") ||
           router_strcasestr_exists(buffer, "hx-request:true");
}

static void router_sanitize_post_name(char *post_name) {
    for (char *p = post_name; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') {
            *p = '\0';
            break;
        }
    }
}

static void router_dispatch(int fd, const char *path, const char *query, int htmx) {
    if (strcmp(path, "/api/gallery") == 0) {
        gallery_serve_json(fd);
    } else if (strcmp(path, "/api/github-status") == 0) {
        github_status_serve(fd);
    } else if (strcmp(path, "/api/github-projects") == 0) {
        github_projects_serve(fd);
    } else if (strcmp(path, "/api/resume") == 0) {
        resume_serve(fd);
    } else if (strcmp(path, "/health") == 0 || strcmp(path, "/api/health") == 0) {
        const char *msg = "{\"status\":\"ok\"}";
        http_send(fd, 200, "application/json", msg, strlen(msg), NULL);
    } else if (strcmp(path, "/partials/gallery-grid.html") == 0 ||
               strcmp(path, "/api/gallery-grid") == 0) {
        gallery_serve_html(fd);
    } else if (strcmp(path, "/blog") == 0) {
        char post_name[256] = {0};
        router_query_param_get(query, "post", post_name, sizeof(post_name));

        if (strlen(post_name) > 0) {
            router_sanitize_post_name(post_name);
            blog_serve_post(fd, post_name, htmx);
        } else {
            static_serve_file(fd, "web/pages/blog.html");
        }
    } else if (strcmp(path, "/") == 0) {
        static_serve_file(fd, "web/pages/index.html");
    } else if (strcmp(path, "/misc") == 0) {
        static_serve_file(fd, "web/pages/misc.html");
    } else if (strcmp(path, "/gallery") == 0) {
        static_serve_file(fd, "web/pages/gallery.html");
    } else if (strncmp(path, "/assets/", 8) == 0 || strncmp(path, "/partials/", 10) == 0) {
        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "web%s", path);
        static_serve_file(fd, file_path);
    } else {
        http_error(fd, 404, "404 Not Found");
    }
}

void *router_handle_client(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    char buffer[ROUTER_BUFFER_SIZE];
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        close(fd);
        return NULL;
    }
    buffer[n] = '\0';

    char method[16], full_path[ROUTER_MAX_PATH], path[ROUTER_MAX_PATH];
    sscanf(buffer, "%15s %511s", method, full_path);

    strncpy(path, full_path, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    char *query = strchr(path, '?');
    if (query) {
        *query = '\0';
        query++;
    }

    int htmx = router_is_htmx_request(buffer);
    printf("[%s] %s%s%s%s\n",
           method,
           path,
           query ? "?" : "",
           query ? query : "",
           htmx ? " (HTMX)" : "");

    router_dispatch(fd, path, query, htmx);
    close(fd);
    return NULL;
}
