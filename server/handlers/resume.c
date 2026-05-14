#include "resume.h"

#include "../core/config.h"
#include "../core/http.h"
#include "../utils/aws.h"
#include "../utils/buffer.h"

#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define RESUME_R2_KEY "resume.pdf"

static size_t resume_curl_write(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    buffer_append((Buffer *)userp, contents, realsize);
    return realsize;
}

void resume_serve(int fd) {
    const Config *config = config_get();

    if (strlen(config->r2_access_key) == 0) {
        http_error(fd, 503, "Resume unavailable");
        return;
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
                         "/" RESUME_R2_KEY,
                         "",
                         date, datetime, host,
                         config->r2_access_key,
                         config->r2_secret_key,
                         auth_header, sizeof(auth_header));

    char url[1024];
    snprintf(url, sizeof(url), "https://%s/" RESUME_R2_KEY, host);

    CURL *curl = curl_easy_init();
    if (!curl) {
        http_error(fd, 500, "curl init failed");
        return;
    }

    struct curl_slist *headers = NULL;
    char auth_h[600], date_h[64];
    snprintf(auth_h, sizeof(auth_h), "Authorization: %s", auth_header);
    snprintf(date_h, sizeof(date_h), "x-amz-date: %s", datetime);
    headers = curl_slist_append(headers, auth_h);
    headers = curl_slist_append(headers, date_h);
    headers = curl_slist_append(headers, "x-amz-content-sha256: UNSIGNED-PAYLOAD");

    Buffer response = {0};
    buffer_init(&response, 1024 * 256);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, resume_curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) {
        buffer_free(&response);
        http_error(fd, 404, "Resume not found");
        return;
    }

    http_send(fd, 200, "application/pdf", response.data, response.size,
              "Content-Disposition: attachment; filename=\"ahmad_jahaf_resume.pdf\"\r\n");

    buffer_free(&response);
}