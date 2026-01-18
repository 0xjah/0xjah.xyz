/*
 * 0xjah.xyz - Minimal C Web Server
 * Build: gcc -o server main.c -lcurl -lpthread -lssl -lcrypto -lmd4c-html -O3 -Wall
 *
 * Dependencies:
 *   - libcurl (HTTP client for R2/GitHub)
 *   - openssl (AWS signature)
 *   - md4c (Markdown parsing) - install: brew install md4c
 *   - pthreads (threading)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <md4c-html.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define BUFFER_SIZE 16384
#define MAX_PATH 512
#define MAX_IMAGES 1000
#define RESPONSE_SIZE 100000

/* ============================================================================
 * Types
 * ============================================================================ */

typedef struct
{
    char port[6];
    char github_repo[256];
    char r2_endpoint[512];
    char r2_access_key[256];
    char r2_secret_key[256];
    char r2_bucket[256];
    char r2_public_url[512];
    char content_dir[256];
} Config;

typedef struct
{
    char *data;
    size_t size;
    size_t capacity;
} Buffer;

typedef struct
{
    char key[256];
    char url[1024];
} GalleryImage;

/* ============================================================================
 * Globals
 * ============================================================================ */

static Config cfg;
static int server_fd = -1;
static volatile sig_atomic_t running = 1;

/* ============================================================================
 * Buffer Utilities
 * ============================================================================ */

static void buffer_init(Buffer *buf, size_t initial_size)
{
    buf->capacity = initial_size;
    buf->size = 0;
    buf->data = malloc(initial_size);
    if (buf->data)
        buf->data[0] = '\0';
}

static void buffer_append(Buffer *buf, const char *data, size_t len)
{
    if (buf->size + len + 1 > buf->capacity)
    {
        buf->capacity = (buf->size + len + 1) * 2;
        buf->data = realloc(buf->data, buf->capacity);
    }
    if (buf->data)
    {
        memcpy(buf->data + buf->size, data, len);
        buf->size += len;
        buf->data[buf->size] = '\0';
    }
}

static void buffer_free(Buffer *buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

/* ============================================================================
 * Curl Write Callback
 * ============================================================================ */

static size_t curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    Buffer *buf = (Buffer *)userp;
    buffer_append(buf, contents, realsize);
    return realsize;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

static void config_load(void)
{
    FILE *fp = fopen(".env", "r");
    if (fp)
    {
        char line[512];
        printf("[Config] Loading .env file\n");

        while (fgets(line, sizeof(line), fp))
        {
            if (line[0] == '#' || line[0] == '\n')
                continue;

            line[strcspn(line, "\n\r")] = '\0';
            char *eq = strchr(line, '=');
            if (eq)
            {
                *eq = '\0';
                setenv(line, eq + 1, 0);
            }
        }
        fclose(fp);
    }

    const char *env;
#define LOAD_ENV(field, name, def) \
    snprintf(cfg.field, sizeof(cfg.field), "%s", (env = getenv(name)) ? env : def)

    LOAD_ENV(port, "PORT", "3000");
    LOAD_ENV(github_repo, "GITHUB_REPO", "0xjah/0xjah.xyz");
    LOAD_ENV(r2_endpoint, "R2_ENDPOINT", "");
    LOAD_ENV(r2_access_key, "R2_ACCESS_KEY", "");
    LOAD_ENV(r2_secret_key, "R2_SECRET_KEY", "");
    LOAD_ENV(r2_bucket, "R2_BUCKET", "");
    LOAD_ENV(r2_public_url, "R2_PUBLIC_URL", "");
    LOAD_ENV(content_dir, "CONTENT_DIR", "content");

#undef LOAD_ENV

    printf("[Config] Server port: %s\n", cfg.port);
    printf("[Config] Content dir: %s\n", cfg.content_dir);
}

/* ============================================================================
 * HTTP Response
 * ============================================================================ */

static const char *mime_type_get(const char *path)
{
    static const struct
    {
        const char *ext;
        const char *mime;
    } types[] = {
        {".html", "text/html; charset=utf-8"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
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
        {NULL, NULL}};

    const char *ext = strrchr(path, '.');
    if (ext)
    {
        for (int i = 0; types[i].ext; i++)
        {
            if (strcasecmp(ext, types[i].ext) == 0)
                return types[i].mime;
        }
    }
    return "application/octet-stream";
}

static void http_send(int fd, int status, const char *content_type,
                      const char *body, size_t len, const char *extra_headers)
{
    char header[2048];
    int hlen = snprintf(header, sizeof(header),
                        "HTTP/1.1 %d %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %zu\r\n"
                        "Connection: keep-alive\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "%s"
                        "\r\n",
                        status, status == 200 ? "OK" : (status == 404 ? "Not Found" : "Error"),
                        content_type, len,
                        extra_headers ? extra_headers : "");

    write(fd, header, hlen);
    if (body && len > 0)
        write(fd, body, len);
}

static void http_error(int fd, int status, const char *message)
{
    http_send(fd, status, "text/plain", message, strlen(message), NULL);
}

/* ============================================================================
 * File Serving
 * ============================================================================ */

static int file_read(const char *path, Buffer *buf)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buffer_init(buf, size + 1);
    if (buf->data)
    {
        fread(buf->data, 1, size, fp);
        buf->size = size;
        buf->data[size] = '\0';
    }

    fclose(fp);
    return buf->data ? 0 : -1;
}

static void file_serve(int fd, const char *path)
{
    Buffer buf = {0};

    if (file_read(path, &buf) < 0)
    {
        http_error(fd, 404, "404 Not Found");
        return;
    }

    http_send(fd, 200, mime_type_get(path), buf.data, buf.size, NULL);
    buffer_free(&buf);
}

/* ============================================================================
 * Markdown Processing
 * ============================================================================ */

static void md_render_cb(const MD_CHAR *html, MD_SIZE size, void *userdata)
{
    Buffer *buf = (Buffer *)userdata;
    buffer_append(buf, html, size);
}

static int markdown_to_html(const char *md_content, Buffer *html_out)
{
    buffer_init(html_out, strlen(md_content) * 2);

    int result = md_html(md_content, strlen(md_content), md_render_cb, html_out,
                         MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH |
                             MD_FLAG_TASKLISTS | MD_FLAG_NOHTML,
                         0);

    return result;
}

static char *frontmatter_skip(char *content)
{
    if (strncmp(content, "---", 3) != 0)
        return content;

    char *end = strstr(content + 3, "\n---");
    if (!end)
        return content;

    return end + 4;
}

/* ============================================================================
 * Blog Handler
 * ============================================================================ */

static void blog_serve_post(int fd, const char *post_name, int htmx_request)
{
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/blog/%s.md", cfg.content_dir, post_name);

    Buffer file_buf = {0};
    if (file_read(path, &file_buf) < 0)
    {
        http_error(fd, 404, "Post not found");
        return;
    }

    char *md_content = frontmatter_skip(file_buf.data);

    Buffer html_buf = {0};
    if (markdown_to_html(md_content, &html_buf) != 0)
    {
        buffer_free(&file_buf);
        http_error(fd, 500, "Markdown parsing failed");
        return;
    }

    Buffer response = {0};
    buffer_init(&response, html_buf.size + 100);
    buffer_append(&response, "<article class=\"blog-post\">", 27);
    buffer_append(&response, html_buf.data, html_buf.size);
    buffer_append(&response, "</article>", 10);

    if (htmx_request)
    {
        http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);
    }
    else
    {
        Buffer page_buf = {0};
        if (file_read("public/blog.html", &page_buf) < 0)
        {
            http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);
        }
        else
        {
            const char *marker = "<div id=\"blog-post-content\" style=\"margin-top: 20px\"></div>";
            char *insert_point = strstr(page_buf.data, marker);

            if (insert_point)
            {
                Buffer full_page = {0};
                buffer_init(&full_page, page_buf.size + response.size);

                size_t prefix_len = insert_point - page_buf.data;
                buffer_append(&full_page, page_buf.data, prefix_len);
                buffer_append(&full_page, "<div id=\"blog-post-content\" style=\"margin-top: 20px\">", 53);
                buffer_append(&full_page, response.data, response.size);
                buffer_append(&full_page, "</div>", 6);
                buffer_append(&full_page, insert_point + strlen(marker),
                              page_buf.size - prefix_len - strlen(marker));

                http_send(fd, 200, "text/html; charset=utf-8", full_page.data, full_page.size, NULL);
                buffer_free(&full_page);
            }
            else
            {
                http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);
            }
            buffer_free(&page_buf);
        }
    }

    buffer_free(&response);
    buffer_free(&html_buf);
    buffer_free(&file_buf);
}

/* ============================================================================
 * Resume Handler
 * ============================================================================ */

static void resume_serve(int fd)
{
    Buffer buf = {0};

    if (file_read("public/static/resume.pdf", &buf) < 0)
    {
        http_error(fd, 404, "Resume not found");
        return;
    }

    http_send(fd, 200, "application/pdf", buf.data, buf.size,
              "Content-Disposition: attachment; filename=\"ahmad_jahaf_resume.pdf\"\r\n");
    buffer_free(&buf);
}

/* ============================================================================
 * AWS Signature (for R2)
 * ============================================================================ */

static void hmac_sha256(const unsigned char *key, int keylen,
                        const unsigned char *data, int datalen,
                        unsigned char *out)
{
    unsigned int len;
    HMAC(EVP_sha256(), key, keylen, data, datalen, out, &len);
}

static void sha256_hex(const char *data, char *out)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)data, strlen(data), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(out + (i * 2), "%02x", hash[i]);
}

static void aws_signature_create(const char *method, const char *uri, const char *query,
                                 const char *date, const char *datetime,
                                 const char *host, char *auth_header)
{
    char canonical_request[4096];
    snprintf(canonical_request, sizeof(canonical_request),
             "%s\n%s\n%s\n"
             "host:%s\n"
             "x-amz-content-sha256:UNSIGNED-PAYLOAD\n"
             "x-amz-date:%s\n\n"
             "host;x-amz-content-sha256;x-amz-date\n"
             "UNSIGNED-PAYLOAD",
             method, uri, query ? query : "", host, datetime);

    char canonical_hash[65];
    sha256_hex(canonical_request, canonical_hash);

    char string_to_sign[1024];
    snprintf(string_to_sign, sizeof(string_to_sign),
             "AWS4-HMAC-SHA256\n%s\n%s/auto/s3/aws4_request\n%s",
             datetime, date, canonical_hash);

    char key[512];
    snprintf(key, sizeof(key), "AWS4%s", cfg.r2_secret_key);

    unsigned char k_date[32], k_region[32], k_service[32], k_signing[32];
    hmac_sha256((unsigned char *)key, strlen(key),
                (unsigned char *)date, strlen(date), k_date);
    hmac_sha256(k_date, 32, (unsigned char *)"auto", 4, k_region);
    hmac_sha256(k_region, 32, (unsigned char *)"s3", 2, k_service);
    hmac_sha256(k_service, 32, (unsigned char *)"aws4_request", 12, k_signing);

    unsigned char signature[32];
    hmac_sha256(k_signing, 32, (unsigned char *)string_to_sign,
                strlen(string_to_sign), signature);

    char sig_hex[65];
    for (int i = 0; i < 32; i++)
        sprintf(sig_hex + (i * 2), "%02x", signature[i]);

    snprintf(auth_header, 512,
             "AWS4-HMAC-SHA256 Credential=%s/%s/auto/s3/aws4_request,"
             "SignedHeaders=host;x-amz-content-sha256;x-amz-date,Signature=%s",
             cfg.r2_access_key, date, sig_hex);
}

/* ============================================================================
 * Gallery Handler
 * ============================================================================ */

static int gallery_fetch_images(GalleryImage *images, int max_images)
{
    if (strlen(cfg.r2_access_key) == 0)
        return 0;

    CURL *curl = curl_easy_init();
    if (!curl)
        return 0;

    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char date[9], datetime[17];
    strftime(date, sizeof(date), "%Y%m%d", tm);
    strftime(datetime, sizeof(datetime), "%Y%m%dT%H%M%SZ", tm);

    char host[1024];
    snprintf(host, sizeof(host), "%s.%s", cfg.r2_bucket, cfg.r2_endpoint);

    char auth_header[512];
    aws_signature_create("GET", "/", "list-type=2", date, datetime, host, auth_header);

    char url[1024];
    snprintf(url, sizeof(url), "https://%s?list-type=2", host);

    struct curl_slist *headers = NULL;
    char auth_h[600], date_h[100];
    snprintf(auth_h, sizeof(auth_h), "Authorization: %s", auth_header);
    snprintf(date_h, sizeof(date_h), "x-amz-date: %s", datetime);
    headers = curl_slist_append(headers, auth_h);
    headers = curl_slist_append(headers, date_h);
    headers = curl_slist_append(headers, "x-amz-content-sha256: UNSIGNED-PAYLOAD");

    Buffer response = {0};
    buffer_init(&response, 4096);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    int count = 0;
    if (res == CURLE_OK && http_code == 200 && response.data)
    {
        char *key_start = response.data;

        while ((key_start = strstr(key_start, "<Key>")) != NULL && count < max_images)
        {
            key_start += 5;
            char *key_end = strstr(key_start, "</Key>");
            if (!key_end)
                break;

            size_t key_len = key_end - key_start;
            if (key_len >= 256)
            {
                key_start = key_end;
                continue;
            }

            char key[256];
            strncpy(key, key_start, key_len);
            key[key_len] = '\0';

            const char *ext = strrchr(key, '.');
            if (!ext || (strcasecmp(ext, ".jpg") != 0 && strcasecmp(ext, ".jpeg") != 0 &&
                         strcasecmp(ext, ".png") != 0 && strcasecmp(ext, ".webp") != 0))
            {
                key_start = key_end;
                continue;
            }

            strcpy(images[count].key, key);
            if (strlen(cfg.r2_public_url) > 0)
            {
                snprintf(images[count].url, sizeof(images[count].url),
                         "https://%s/%s", cfg.r2_public_url, key);
            }
            else
            {
                snprintf(images[count].url, sizeof(images[count].url),
                         "https://%s/%s", host, key);
            }
            count++;
            key_start = key_end;
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    buffer_free(&response);

    /* Sort alphabetically */
    for (int i = 0; i < count - 1; i++)
    {
        for (int j = i + 1; j < count; j++)
        {
            if (strcmp(images[i].key, images[j].key) > 0)
            {
                GalleryImage temp = images[i];
                images[i] = images[j];
                images[j] = temp;
            }
        }
    }

    return count;
}

static void gallery_serve_json(int fd)
{
    GalleryImage *images = malloc(MAX_IMAGES * sizeof(GalleryImage));
    int count = gallery_fetch_images(images, MAX_IMAGES);

    Buffer response = {0};
    buffer_init(&response, 16384);
    buffer_append(&response, "{\"images\":[", 11);

    for (int i = 0; i < count; i++)
    {
        char entry[2048];
        int len = snprintf(entry, sizeof(entry),
                           "%s{\"name\":\"%s\",\"key\":\"%s\",\"url\":\"%s\"}",
                           i > 0 ? "," : "", images[i].key, images[i].key, images[i].url);
        buffer_append(&response, entry, len);
    }

    char footer[32];
    int flen = snprintf(footer, sizeof(footer), "],\"count\":%d}", count);
    buffer_append(&response, footer, flen);

    http_send(fd, 200, "application/json", response.data, response.size, NULL);

    buffer_free(&response);
    free(images);
}

static void gallery_serve_html(int fd)
{
    GalleryImage *images = malloc(MAX_IMAGES * sizeof(GalleryImage));
    int count = gallery_fetch_images(images, MAX_IMAGES);

    Buffer response = {0};
    buffer_init(&response, RESPONSE_SIZE);

    char header[256];
    int hlen = snprintf(header, sizeof(header),
                        "<div class=\"gallery-stats\">"
                        "<i class=\"fas fa-images\"></i> %d image%s found"
                        "</div><div class=\"gallery-grid\">",
                        count, count != 1 ? "s" : "");
    buffer_append(&response, header, hlen);

    for (int i = 0; i < count; i++)
    {
        char item[2048];
        int len = snprintf(item, sizeof(item),
                           "<div class=\"gallery-item\" onclick=\"openModal('%s')\">"
                           "<img src=\"%s\" alt=\"%s\" loading=\"lazy\" decoding=\"async\" />"
                           "<div class=\"gallery-item-overlay\">"
                           "<p class=\"gallery-item-title\">%s</p>"
                           "</div></div>",
                           images[i].url, images[i].url, images[i].key, images[i].key);
        buffer_append(&response, item, len);
    }

    buffer_append(&response, "</div>", 6);

    http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);

    buffer_free(&response);
    free(images);
}

/* ============================================================================
 * GitHub Status Handler
 * ============================================================================ */

static void format_relative_time(const char *iso_time, char *output, size_t size)
{
    struct tm tm_time = {0};

    if (sscanf(iso_time, "%d-%d-%dT%d:%d:%d",
               &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
               &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec) != 6)
    {
        snprintf(output, size, "recently");
        return;
    }

    tm_time.tm_year -= 1900;
    tm_time.tm_mon -= 1;

    time_t pushed = timegm(&tm_time);
    time_t now = time(NULL);
    int diff = (int)difftime(now, pushed);

    if (diff < 60)
        snprintf(output, size, "just now");
    else if (diff < 3600)
        snprintf(output, size, "%dm ago", diff / 60);
    else if (diff < 86400)
        snprintf(output, size, "%dh ago", diff / 3600);
    else if (diff < 604800)
        snprintf(output, size, "%dd ago", diff / 86400);
    else if (diff < 2592000)
        snprintf(output, size, "%dw ago", diff / 604800);
    else if (diff < 31536000)
        snprintf(output, size, "%dmo ago", diff / 2592000);
    else
        snprintf(output, size, "%dy ago", diff / 31536000);
}

static void github_status_serve(int fd)
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        const char *msg = "<div><h2>Status</h2><p>Last updated: unknown</p></div>";
        http_send(fd, 200, "text/html", msg, strlen(msg), NULL);
        return;
    }

    Buffer response = {0};
    buffer_init(&response, 4096);

    char url[512];
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s", cfg.github_repo);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: 0xjah.xyz/1.0");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);

    char time_str[64] = "recently";
    if (res == CURLE_OK && response.data)
    {
        char *pushed = strstr(response.data, "\"pushed_at\":\"");
        if (pushed)
        {
            pushed += 13;
            char *end = strchr(pushed, '"');
            if (end && (end - pushed) < 64)
            {
                char iso_time[64];
                strncpy(iso_time, pushed, end - pushed);
                iso_time[end - pushed] = '\0';
                format_relative_time(iso_time, time_str, sizeof(time_str));
            }
        }
    }

    char html[512];
    int len = snprintf(html, sizeof(html),
                       "<div class=\"status-header\"><h2>Status</h2>"
                       "<p class=\"quote\">Last updated: %s</p></div>",
                       time_str);

    http_send(fd, 200, "text/html; charset=utf-8", html, len, NULL);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    buffer_free(&response);
}

/* ============================================================================
 * Query String Parser
 * ============================================================================ */

static void query_param_get(const char *query, const char *param, char *value, size_t size)
{
    value[0] = '\0';
    if (!query || !param)
        return;

    size_t param_len = strlen(param);
    const char *p = query;

    while ((p = strstr(p, param)) != NULL)
    {
        if ((p == query || *(p - 1) == '&') && *(p + param_len) == '=')
        {
            p += param_len + 1;
            const char *end = strchr(p, '&');
            size_t len = end ? (size_t)(end - p) : strlen(p);
            if (len >= size)
                len = size - 1;
            strncpy(value, p, len);
            value[len] = '\0';
            return;
        }
        p++;
    }
}

/* ============================================================================
 * Request Router
 * ============================================================================ */

static int is_htmx_request(const char *buffer)
{
    return strstr(buffer, "HX-Request: true") != NULL;
}

static void *handle_client(void *arg)
{
    int fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    if (n <= 0)
    {
        close(fd);
        return NULL;
    }
    buffer[n] = '\0';

    char method[16], full_path[MAX_PATH], path[MAX_PATH];
    sscanf(buffer, "%15s %511s", method, full_path);

    /* Separate path and query */
    strncpy(path, full_path, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    char *query = strchr(path, '?');
    if (query)
    {
        *query = '\0';
        query++;
    }

    int htmx = is_htmx_request(buffer);
    printf("[%s] %s%s%s%s\n", method, path,
           query ? "?" : "", query ? query : "",
           htmx ? " (HTMX)" : "");

    /* ===== API Routes ===== */
    if (strcmp(path, "/api/gallery") == 0)
    {
        gallery_serve_json(fd);
    }
    else if (strcmp(path, "/api/github-status") == 0)
    {
        github_status_serve(fd);
    }
    else if (strcmp(path, "/api/resume") == 0)
    {
        resume_serve(fd);
    }
    else if (strcmp(path, "/health") == 0 || strcmp(path, "/api/health") == 0)
    {
        const char *msg = "{\"status\":\"ok\"}";
        http_send(fd, 200, "application/json", msg, strlen(msg), NULL);
    }
    /* ===== Gallery Grid Partial ===== */
    else if (strcmp(path, "/partials/gallery/grid.html") == 0 ||
             strcmp(path, "/api/gallery-grid") == 0)
    {
        gallery_serve_html(fd);
    }
    /* ===== Blog Routes ===== */
    else if (strcmp(path, "/blog") == 0)
    {
        char post_name[256] = {0};
        query_param_get(query, "post", post_name, sizeof(post_name));

        if (strlen(post_name) > 0)
        {
            /* Sanitize post name */
            for (char *p = post_name; *p; p++)
            {
                if (!isalnum(*p) && *p != '_' && *p != '-')
                {
                    *p = '\0';
                    break;
                }
            }
            blog_serve_post(fd, post_name, htmx);
        }
        else
        {
            file_serve(fd, "public/blog.html");
        }
    }
    /* ===== Page Routes ===== */
    else if (strcmp(path, "/") == 0)
    {
        file_serve(fd, "public/index.html");
    }
    else if (strcmp(path, "/misc") == 0)
    {
        file_serve(fd, "public/misc.html");
    }
    else if (strcmp(path, "/gallery") == 0)
    {
        file_serve(fd, "public/gallery.html");
    }
    /* ===== Static Assets ===== */
    else if (strncmp(path, "/static/", 8) == 0)
    {
        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "public%s", path);
        file_serve(fd, file_path);
    }
    /* ===== Partials ===== */
    else if (strncmp(path, "/partials/", 10) == 0)
    {
        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "public%s", path);
        file_serve(fd, file_path);
    }
    /* ===== 404 ===== */
    else
    {
        http_error(fd, 404, "404 Not Found");
    }

    close(fd);
    return NULL;
}

/* ============================================================================
 * Signal Handler
 * ============================================================================ */

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
    if (server_fd >= 0)
        close(server_fd);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    config_load();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(atoi(cfg.port))};

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 1000) < 0)
    {
        perror("listen");
        return 1;
    }

    printf("\n");
    printf("  ╭─────────────────────────────────────╮\n");
    printf("  │  0xjah.xyz server                   │\n");
    printf("  │  http://0.0.0.0:%-5s               │\n", cfg.port);
    printf("  ╰─────────────────────────────────────╯\n");
    printf("\n");

    while (running)
    {
        socklen_t addrlen = sizeof(addr);
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen);

        if (*client_fd < 0)
        {
            free(client_fd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }

    curl_global_cleanup();
    printf("\nServer stopped.\n");
    return 0;
}
