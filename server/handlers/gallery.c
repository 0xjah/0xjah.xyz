#include "gallery.h"

#include "../core/config.h"
#include "../core/http.h"
#include "../utils/aws.h"
#include "../utils/buffer.h"

#include <curl/curl.h>

#include <pthread.h>
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
    char thumb[1024]; // thumbs/<key>.webp if deploy/make-thumbs.py made one, else the original
} GalleryImage;

#define GALLERY_THUMB_PREFIX "thumbs/"

static size_t gallery_curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    Buffer *buffer = (Buffer *)userp;
    buffer_append(buffer, contents, realsize);
    return realsize;
}

static int gallery_fetch_images_r2(GalleryImage *images, int max_images) {
    const Config *config = config_get();

    if (strlen(config->r2_access_key) == 0) {
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
    snprintf(host, sizeof(host), "%s.%s", config->r2_bucket, config->r2_endpoint);

    char auth_header[512];
    aws_signature_create("GET",
                         "/",
                         "list-type=2",
                         date,
                         datetime,
                         host,
                         config->r2_access_key,
                         config->r2_secret_key,
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
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gallery_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    int count = 0;
    char (*thumbs)[256] = malloc(GALLERY_MAX_IMAGES * sizeof(*thumbs));
    int thumb_count = 0;
    if (res == CURLE_OK && http_code == 200 && response.data && thumbs) {
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

            if (strncmp(key, GALLERY_THUMB_PREFIX, strlen(GALLERY_THUMB_PREFIX)) == 0) {
                if (thumb_count < GALLERY_MAX_IMAGES) {
                    strcpy(thumbs[thumb_count++], key);
                }
                key_start = key_end;
                continue;
            }

            const char *ext = strrchr(key, '.');
            if (!ext ||
                (strcasecmp(ext, ".jpg") != 0 && strcasecmp(ext, ".jpeg") != 0 &&
                 strcasecmp(ext, ".png") != 0 && strcasecmp(ext, ".webp") != 0)) {
                key_start = key_end;
                continue;
            }

            strcpy(images[count].key, key);
            if (strlen(config->r2_public_url) > 0) {
                snprintf(images[count].url, sizeof(images[count].url),
                         "https://%s/%s", config->r2_public_url, key);
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

    const char *base = strlen(config->r2_public_url) > 0 ? config->r2_public_url : host;
    for (int i = 0; i < count; i++) {
        strcpy(images[i].thumb, images[i].url);
        char want[300];
        snprintf(want, sizeof(want), GALLERY_THUMB_PREFIX "%s.webp", images[i].key);
        for (int t = 0; t < thumb_count; t++) {
            if (strcmp(thumbs[t], want) == 0) {
                snprintf(images[i].thumb, sizeof(images[i].thumb), "https://%s/%s", base, want);
                break;
            }
        }
    }
    free(thumbs);

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

// Listing R2 takes ~300ms, so keep the list in memory. Once it is older than
// GALLERY_CACHE_TTL, requests still get the cached list instantly and a background
// thread refreshes it (stale-while-revalidate).
#ifndef GALLERY_CACHE_TTL
#define GALLERY_CACHE_TTL 300
#endif

static pthread_mutex_t gallery_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static GalleryImage *gallery_cache; // NULL until the first successful listing
static int gallery_cache_count;
static time_t gallery_cache_time;
static int gallery_cache_refreshing;

static void *gallery_cache_refresh(void *arg) {
    (void)arg;
    GalleryImage *fresh = malloc(GALLERY_MAX_IMAGES * sizeof(GalleryImage));
    int count = fresh ? gallery_fetch_images_r2(fresh, GALLERY_MAX_IMAGES) : 0;

    pthread_mutex_lock(&gallery_cache_lock);
    if (fresh && count > 0) { // on failure keep serving the old list
        GalleryImage *old = gallery_cache;
        gallery_cache = fresh;
        gallery_cache_count = count;
        fresh = old;
    }
    gallery_cache_time = time(NULL); // also after a failure, so a broken R2 isn't hit every request
    gallery_cache_refreshing = 0;
    pthread_mutex_unlock(&gallery_cache_lock);

    free(fresh);
    return NULL;
}

static int gallery_fetch_images(GalleryImage *images, int max_images) {
    pthread_mutex_lock(&gallery_cache_lock);
    if (!gallery_cache) {
        // Cold start: fetch synchronously; concurrent first requests wait on the lock.
        GalleryImage *fresh = malloc(GALLERY_MAX_IMAGES * sizeof(GalleryImage));
        int count = fresh ? gallery_fetch_images_r2(fresh, GALLERY_MAX_IMAGES) : 0;
        if (count > 0) {
            gallery_cache = fresh;
            gallery_cache_count = count;
            gallery_cache_time = time(NULL);
        } else {
            free(fresh); // retry on the next request
        }
    } else if (!gallery_cache_refreshing && time(NULL) - gallery_cache_time >= GALLERY_CACHE_TTL) {
        pthread_t tid;
        gallery_cache_refreshing = 1;
        if (pthread_create(&tid, NULL, gallery_cache_refresh, NULL) == 0) {
            pthread_detach(tid);
        } else {
            gallery_cache_refreshing = 0;
        }
    }

    int count = gallery_cache_count < max_images ? gallery_cache_count : max_images;
    if (gallery_cache) {
        memcpy(images, gallery_cache, (size_t)count * sizeof(GalleryImage));
    } else {
        count = 0;
    }
    pthread_mutex_unlock(&gallery_cache_lock);
    return count;
}

void gallery_serve_json(int fd) {
    GalleryImage *images = malloc(GALLERY_MAX_IMAGES * sizeof(GalleryImage));
    if (!images) {
        http_error(fd, 500, "Something broke while loading the gallery. Try again later.");
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
        if (len < 0 || (size_t)len >= sizeof(entry)) {
            continue; // truncated: skip rather than append past the buffer
        }
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
        http_error(fd, 500, "Something broke while loading the gallery. Try again later.");
        return;
    }

    int count = gallery_fetch_images(images, GALLERY_MAX_IMAGES);

    Buffer response = {0};
    buffer_init(&response, GALLERY_RESPONSE_SIZE);

    char header[256];
    int hlen = snprintf(header,
                        sizeof(header),
                        "<div class=\"gallery-stats\">"
                        "%d image%s found"
                        "</div><div class=\"gallery-grid\">",
                        count,
                        count != 1 ? "s" : "");
    buffer_append(&response, header, (size_t)hlen);

    for (int i = 0; i < count; i++) {
        char item[4096];
        int len = snprintf(item,
                           sizeof(item),
                           "<div class=\"gallery-item\" onclick=\"openModal('%s', '%s')\">"
                           "<img src=\"%s\" alt=\"%s\" %s decoding=\"async\" "
                           "onload=\"this.parentNode.classList.add('loaded')\" "
                           "onerror=\"this.parentNode.classList.add('loaded')\" />"
                           "<div class=\"gallery-item-overlay\">"
                           "<p class=\"gallery-item-title\">%s</p>"
                           "</div></div>",
                           images[i].url,
                           images[i].key,
                           images[i].thumb,
                           images[i].key,
                           i < 4 ? "fetchpriority=\"high\"" : "loading=\"lazy\"",
                           images[i].key);
        if (len < 0 || (size_t)len >= sizeof(item)) {
            continue; // truncated: skip rather than append past the buffer
        }
        buffer_append(&response, item, (size_t)len);
    }

    buffer_append(&response, "</div>", 6);
    http_send(fd, 200, "text/html; charset=utf-8", response.data, response.size, NULL);

    buffer_free(&response);
    free(images);
}
