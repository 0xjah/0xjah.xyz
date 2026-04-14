#include "github.h"

#include "../core/config.h"
#include "../core/http.h"
#include "../utils/buffer.h"

#include <curl/curl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static size_t github_curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    Buffer *buffer = (Buffer *)userp;
    buffer_append(buffer, contents, realsize);
    return realsize;
}

static void github_format_relative_time(const char *iso_time, char *output, size_t size) {
    struct tm tm_time = {0};

    if (sscanf(iso_time,
               "%d-%d-%dT%d:%d:%d",
               &tm_time.tm_year,
               &tm_time.tm_mon,
               &tm_time.tm_mday,
               &tm_time.tm_hour,
               &tm_time.tm_min,
               &tm_time.tm_sec) != 6) {
        snprintf(output, size, "recently");
        return;
    }

    tm_time.tm_year -= 1900;
    tm_time.tm_mon -= 1;

    time_t pushed = timegm(&tm_time);
    time_t now = time(NULL);
    int diff = (int)difftime(now, pushed);

    if (diff < 60) {
        snprintf(output, size, "just now");
    } else if (diff < 3600) {
        snprintf(output, size, "%dm ago", diff / 60);
    } else if (diff < 86400) {
        snprintf(output, size, "%dh ago", diff / 3600);
    } else if (diff < 604800) {
        snprintf(output, size, "%dd ago", diff / 86400);
    } else if (diff < 2592000) {
        snprintf(output, size, "%dw ago", diff / 604800);
    } else if (diff < 31536000) {
        snprintf(output, size, "%dmo ago", diff / 2592000);
    } else {
        snprintf(output, size, "%dy ago", diff / 31536000);
    }
}

void github_status_serve(int fd) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        const char *msg = "<div><h2>Status</h2><p>Last updated: unknown</p></div>";
        http_send(fd, 200, "text/html", msg, strlen(msg), NULL);
        return;
    }

    const Config *config = config_get();
    Buffer response = {0};
    buffer_init(&response, 4096);

    char url[512];
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s", config->github_repo);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: 0xjah.xyz/1.0");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, github_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);

    char time_str[64] = "recently";
    if (res == CURLE_OK && response.data) {
        char *pushed = strstr(response.data, "\"pushed_at\":\"");
        if (pushed) {
            pushed += 13;
            char *end = strchr(pushed, '"');
            if (end && (end - pushed) < 64) {
                char iso_time[64];
                strncpy(iso_time, pushed, (size_t)(end - pushed));
                iso_time[end - pushed] = '\0';
                github_format_relative_time(iso_time, time_str, sizeof(time_str));
            }
        }
    }

    char html[512];
    int len = snprintf(html,
                       sizeof(html),
                       "<div class=\"status-header\"><h2>Status</h2>"
                       "<p class=\"quote\">Last updated: %s</p></div>",
                       time_str);

    http_send(fd, 200, "text/html; charset=utf-8", html, (size_t)len, NULL);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    buffer_free(&response);
}

void github_projects_serve(int fd) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        const char *msg = "<li>Projects unavailable</li>";
        http_send(fd, 200, "text/html", msg, strlen(msg), NULL);
        return;
    }

    Buffer response = {0};
    buffer_init(&response, 16384);

    char url[512];
    snprintf(url, sizeof(url),
             "https://api.github.com/users/0xjah/repos?sort=stars&per_page=5&type=owner");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: 0xjah.xyz/1.0");
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, github_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);

    CURLcode res = curl_easy_perform(curl);

    Buffer html = {0};
    buffer_init(&html, 4096);

    if (res == CURLE_OK && response.data) {
        char *p = response.data;
        int count = 0;

        while (count < 5) {
            char *html_url_key = strstr(p, "\"html_url\":\"https://github.com/0xjah/");
            if (!html_url_key) {
                break;
            }

            char *url_val = html_url_key + 11;
            char *url_end = strchr(url_val, '"');
            if (!url_end) {
                break;
            }

            char repo_url[256] = {0};
            size_t url_len = (size_t)(url_end - url_val);
            if (url_len >= sizeof(repo_url)) {
                p = url_end + 1;
                continue;
            }
            strncpy(repo_url, url_val, url_len);

            const char *prefix = "https://github.com/0xjah/";
            if (strncmp(repo_url, prefix, strlen(prefix)) != 0 ||
                strlen(repo_url) <= strlen(prefix)) {
                p = url_end + 1;
                continue;
            }

            char name[128] = {0};
            char *search = p;
            char *last_name_start = NULL;
            while (search < html_url_key) {
                char *found = strstr(search, "\"name\":\"");
                if (!found || found >= html_url_key) {
                    break;
                }
                last_name_start = found + 8;
                search = found + 1;
            }
            if (last_name_start) {
                char *name_end = strchr(last_name_start, '"');
                if (name_end) {
                    size_t nlen = (size_t)(name_end - last_name_start);
                    if (nlen < sizeof(name)) {
                        strncpy(name, last_name_start, nlen);
                    }
                }
            }

            if (strlen(name) == 0) {
                p = url_end + 1;
                continue;
            }

            char *next_obj = strstr(url_end + 1, "\"html_url\":\"https://github.com/0xjah/");

            char desc[256] = {0};
            char *desc_key = strstr(url_end + 1, "\"description\":");
            if (desc_key && (!next_obj || desc_key < next_obj)) {
                desc_key += 14;
                if (*desc_key == '"') {
                    desc_key++;
                    char *desc_end = strchr(desc_key, '"');
                    if (desc_end) {
                        size_t dlen = (size_t)(desc_end - desc_key);
                        if (dlen >= sizeof(desc)) {
                            dlen = sizeof(desc) - 1;
                        }
                        strncpy(desc, desc_key, dlen);
                    }
                }
            }

            int stars = 0;
            char *stars_key = strstr(url_end + 1, "\"stargazers_count\":");
            if (stars_key && (!next_obj || stars_key < next_obj)) {
                stars = atoi(stars_key + 19);
            }

            char item[1024];
            int ilen = snprintf(item,
                                sizeof(item),
                                "<li>"
                                "<a href=\"%s\" "
                                "onclick=\"event.stopPropagation();window.open('%s','_blank');return false;\" "
                                "class=\"project-link\">"
                                "%s%s%s"
                                "</a>"
                                "<span style=\"color:var(--muted);font-size:12px;margin-left:8px\">&#9733; %d</span>"
                                "</li>",
                                repo_url,
                                repo_url,
                                name,
                                strlen(desc) > 0 ? " &mdash; " : "",
                                desc,
                                stars);
            buffer_append(&html, item, (size_t)ilen);
            count++;

            p = url_end + 1;
        }

        if (html.size == 0) {
            const char *fallback = "<li style=\"color:var(--muted)\">No public repos found.</li>";
            buffer_append(&html, fallback, strlen(fallback));
        }
    } else {
        const char *err = "<li style=\"color:var(--error)\">Could not load projects.</li>";
        buffer_append(&html, err, strlen(err));
    }

    const char *more = "<li>And more on "
                       "<a href=\"https://github.com/0xjah\" "
                       "onclick=\"event.stopPropagation();window.open('https://github.com/0xjah','_blank');return false;\" "
                       "class=\"project-link\">GitHub</a>"
                       "</li>";
    buffer_append(&html, more, strlen(more));

    http_send(fd, 200, "text/html; charset=utf-8", html.data, html.size, NULL);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    buffer_free(&response);
    buffer_free(&html);
}
