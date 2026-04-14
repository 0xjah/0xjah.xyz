#include "gallery.h"

#include "../core/config.h"
#include "../core/http.h"
#include "../utils/aws.h"
#include "../utils/buffer.h"

#include <curl/curl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define GALLERY_MAX_IMAGES 1000
#define GALLERY_RESPONSE_SIZE 100000

typedef struct {
    char key[256];
    char url[1024];
} GalleryImage;

static size_t gallery_curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    Buffer *buf = (Buffer *)userp;
    buffer_append(buf, contents, realsize);
    return realsize;
}

static int gallery_fetch_images(GalleryImage *images, int max_images) {
    const Config *cfg = config_get();

    if (strlen(cfg->r2_access_key) == 0) {
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        return 0;
    }

    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char date[9], datetime[17];
    strftime(date, sizeof(date), "%Y%m%d", tm);
    strftime(datetime, sizeof(datetime), "%Y%m%dT%H%M%SZ", tm);

    char host[1024];
    snprintf(host, sizeof(host), "%s.%s", cfg->r2_bucket, cfg->r2_endpoint);

    char auth_header[512];
    aws_signature_create("GET",
                         "/",
                         "list-type=2",
                         date,
                         datetime,
                         host,
                         cfg->r2_access_key,
                         cfg->r2_secret_key,
                         auth_header,
                         sizeof(auth_header));

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
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gallery_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    int count = 0;
    if (res == CURLE_OK && http_code == 200 && response.data) {
        char *key_start = response.data;

        while ((key_start = strstr(key_start, "<Key>")) != NULL && count < max_images) {
            key_start += 5;
            char *key_end = strstr(key_start, "</Key>");
            if (!key_end) {
                break;
            }

            size_t key_len = (size_t)(key_end - key_start);
            if (key_len >= 256) {
                key_start = key_end;
                continue;
            }

            char key[256];
            strncpy(key, key_start, key_len);
            key[key_len] = '\0';

            const char *ext = strrchr(key, '.');
            if (!ext ||
                (strcasecmp(ext, ".jpg") != 0 && strcasecmp(ext, ".jpeg") != 0 &&
                 strcasecmp(ext, ".png") != 0 && strcasecmp(ext, ".webp") != 0)) {
                key_start = key_end;
                continue;
            }

            strcpy(images[count].key, key);
            if (strlen(cfg->r2_public_url) > 0) {
                snprintf(images[count].url, sizeof(images[count].url),
                         "https://%s/%s", cfg->r2_public_url, key);
            } else {
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

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strcmp(images[i].key, images[j].key) > 0) {
                GalleryImage temp = images[i];
                images[i] = images[j];
                images[j] = temp;
            }
        }
    }

    return count;
}

void gallery_serve_json(int fd) {
    GalleryImage *images = malloc(GALLERY_MAX_IMAGES * sizeof(GalleryImage));
    if (!images) {
        http_error(fd, 500, "Gallery memory allocation failed");
        return;
    }

    int count = gallery_fetch_images(images, GALLERY_MAX_IMAGES);

    Buffer response = {0};
    buffer_init(&response, 16384);
    buffer_append(&response, "{\"images\":[", 11);

    for (int i = 0; i < count; i++) {
        char entry[2048];
        int len = snprintf(entry,
                           sizeof(entry),
                           "%s{\"name\":\"%s\",\"key\":\"%s\",\"url\":\"%s\"}",
                           i > 0 ? "," : "", images[i].key, images[i].key, images[i].url);
        buffer_append(&response, entry, (size_t)len);
    }

    char footer[32];
    int flen = snprintf(footer, sizeof(footer), "],\"count\":%d}", count);
    buffer_append(&response, footer, (size_t)flen);

    http_send(fd, 200, "application/json", response.data, response.size, NULL);

    buffer_free(&response);
    free(images);
}

void gallery_serve_html(int fd) {
    GalleryImage *images = malloc(GALLERY_MAX_IMAGES * sizeof(GalleryImage));
    if (!images) {
        http_error(fd, 500, "Gallery memory allocation failed");
        return;
    }

    int count = gallery_fetch_images(images, GALLERY_MAX_IMAGES);

    Buffer response = {0};
    buffer_init(&response, GALLERY_RESPONSE_SIZE);

    char header[256];
    int hlen = snprintf(header,
                        sizeof(header),
                        "<div class=\"gallery-stats\">"
                        "<i class=\"fas fa-images\"></i> %d image%s found"
                        "</div><div class=\"gallery-grid\">",
                        count,
                        count != 1 ? "s" : "");
    buffer_append(&response, header, (size_t)hlen);

    for (int i = 0; i < count; i++) {
        char item[2048];
        int len = snprintf(item,
                           sizeof(item),
                           "<div class=\"gallery-item\" onclick=\"openModal('%s')\">"
                           "<img src=\"%s\" alt=\"%s\" loading=\"lazy\" decoding=\"async\" />"
                           "<div class=\"gallery-item-overlay\">"
                           "<p class=\"gallery-item-title\">%s</p>"
                           "</div></div>",
                           images[i].url,
                           images[i].url,
                           images[i].key,
                           images[i].key);
        buffer_append(&response, item, (size_t)len);
    }

    buffer_append(&response, "</div>", 6);
    http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);

    buffer_free(&response);
    free(images);
}
