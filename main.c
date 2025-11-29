/*
 * 0xjah.me - Personal Portfolio Web Server
 * Written in C for maximum performance and minimal dependencies
 * 
 * Build macOS: gcc -o main main.c -lcurl -lpthread -O3 -Wall
 * Run: ./main
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <ctype.h>

#define PORT 3000
#define MAX_CONNECTIONS 1000
#define BUFFER_SIZE 16384
#define MAX_PATH 512
#define STATIC_CACHE_TTL 31536000
#define HTML_CACHE_TTL 3600

/* Configuration structure */
typedef struct {
    char port[6];
    char host[256];
    char environment[32];
    char github_repo[256];
    char github_token[256];
    char discord_id[64];
    char fallback_status[256];
    char r2_endpoint[512];
    char r2_access_key[256];
    char r2_secret_key[256];
    char r2_bucket[256];
    char r2_public_url[512];
    int static_cache_ttl;
    int html_cache_ttl;
} Config;

/* Global configuration */
Config config;
int server_fd;
volatile sig_atomic_t running = 1;

/* CURL response structure */
struct MemoryStruct {
    char *memory;
    size_t size;
};

/* Initialize configuration from environment */
void load_config() {
    char *env_val;
    
    snprintf(config.port, sizeof(config.port), "%s", 
             (env_val = getenv("PORT")) ? env_val : "3000");
    snprintf(config.host, sizeof(config.host), "%s", 
             (env_val = getenv("HOST")) ? env_val : "0.0.0.0");
    snprintf(config.environment, sizeof(config.environment), "%s", 
             (env_val = getenv("ENV")) ? env_val : "development");
    snprintf(config.github_repo, sizeof(config.github_repo), "%s", 
             (env_val = getenv("GITHUB_REPO")) ? env_val : "0xjah/0xjah.xyz");
    snprintf(config.github_token, sizeof(config.github_token), "%s", 
             (env_val = getenv("GITHUB_TOKEN")) ? env_val : "");
    snprintf(config.discord_id, sizeof(config.discord_id), "%s", 
             (env_val = getenv("DISCORD_ID")) ? env_val : "763769303681335316");
    snprintf(config.fallback_status, sizeof(config.fallback_status), "%s", 
             (env_val = getenv("FALLBACK_STATUS")) ? env_val : "online • coding");
    snprintf(config.r2_endpoint, sizeof(config.r2_endpoint), "%s", 
             (env_val = getenv("R2_ENDPOINT")) ? env_val : "");
    snprintf(config.r2_access_key, sizeof(config.r2_access_key), "%s", 
             (env_val = getenv("R2_ACCESS_KEY")) ? env_val : "");
    snprintf(config.r2_secret_key, sizeof(config.r2_secret_key), "%s", 
             (env_val = getenv("R2_SECRET_KEY")) ? env_val : "");
    snprintf(config.r2_bucket, sizeof(config.r2_bucket), "%s", 
             (env_val = getenv("R2_BUCKET")) ? env_val : "");
    snprintf(config.r2_public_url, sizeof(config.r2_public_url), "%s", 
             (env_val = getenv("R2_PUBLIC_URL")) ? env_val : "");
    
    config.static_cache_ttl = (env_val = getenv("STATIC_CACHE_TTL")) ? 
                              atoi(env_val) : STATIC_CACHE_TTL;
    config.html_cache_ttl = (env_val = getenv("HTML_CACHE_TTL")) ? 
                            atoi(env_val) : HTML_CACHE_TTL;
    
    printf("Server configuration loaded - Port: %s, Environment: %s\n", 
           config.port, config.environment);
}

/* CURL write callback */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;
    
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

/* Get file size */
long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) return st.st_size;
    return -1;
}

/* Get content type from file extension */
const char* get_content_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "text/plain";
    
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".webp") == 0) return "image/webp";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    
    return "application/octet-stream";
}

/* URL decode */
void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && 
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('B' - 10);
            else b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

/* Send HTTP response */
void send_response(int client_fd, int status_code, const char *status_text,
                  const char *content_type, const char *body, size_t body_len,
                  int cache_ttl) {
    char header[BUFFER_SIZE];
    int header_len;
    
    header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: keep-alive\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\n"
        "X-XSS-Protection: 1; mode=block\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        status_code, status_text, content_type, body_len);
    
    if (cache_ttl > 0) {
        header_len += snprintf(header + header_len, sizeof(header) - header_len,
            "Cache-Control: public, max-age=%d, stale-while-revalidate=86400\r\n",
            cache_ttl);
    } else {
        header_len += snprintf(header + header_len, sizeof(header) - header_len,
            "Cache-Control: no-cache, must-revalidate\r\n");
    }
    
    header_len += snprintf(header + header_len, sizeof(header) - header_len, "\r\n");
    
    write(client_fd, header, header_len);
    if (body && body_len > 0) {
        write(client_fd, body, body_len);
    }
}

/* Serve static file */
void serve_file(int client_fd, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("File not found: %s\n", path);
        const char *msg = "404 Not Found";
        send_response(client_fd, 404, "Not Found", "text/plain", msg, strlen(msg), 0);
        return;
    }
    
    long size = get_file_size(path);
    if (size < 0) {
        fclose(fp);
        const char *msg = "500 Internal Server Error";
        send_response(client_fd, 500, "Internal Server Error", "text/plain", 
                     msg, strlen(msg), 0);
        return;
    }
    
    char *content = malloc(size);
    if (!content) {
        fclose(fp);
        const char *msg = "500 Internal Server Error";
        send_response(client_fd, 500, "Internal Server Error", "text/plain", 
                     msg, strlen(msg), 0);
        return;
    }
    
    fread(content, 1, size, fp);
    fclose(fp);
    
    const char *content_type = get_content_type(path);
    int cache_ttl = (strstr(path, ".html") != NULL) ? 
                    config.html_cache_ttl : config.static_cache_ttl;
    
    send_response(client_fd, 200, "OK", content_type, content, size, cache_ttl);
    free(content);
}

/* Get relative time string */
void get_relative_time(time_t timestamp, char *buffer, size_t size) {
    time_t now = time(NULL);
    time_t diff = now - timestamp;
    
    if (diff < 60) {
        snprintf(buffer, size, "just now");
    } else if (diff < 3600) {
        int minutes = diff / 60;
        snprintf(buffer, size, "%d minute%s ago", minutes, minutes == 1 ? "" : "s");
    } else if (diff < 86400) {
        int hours = diff / 3600;
        snprintf(buffer, size, "%d hour%s ago", hours, hours == 1 ? "" : "s");
    } else if (diff < 604800) {
        int days = diff / 86400;
        snprintf(buffer, size, "%d day%s ago", days, days == 1 ? "" : "s");
    } else if (diff < 2592000) {
        int weeks = diff / 604800;
        snprintf(buffer, size, "%d week%s ago", weeks, weeks == 1 ? "" : "s");
    } else {
        int months = diff / 2592000;
        snprintf(buffer, size, "%d month%s ago", months, months == 1 ? "" : "s");
    }
}

/* Parse ISO8601 timestamp */
time_t parse_iso8601(const char *timestamp) {
    struct tm tm = {0};
    sscanf(timestamp, "%d-%d-%dT%d:%d:%d",
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return mktime(&tm);
}

/* Handle GitHub last updated HTML endpoint */
void handle_github_last_updated_html(int client_fd) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        const char *msg = "error fetching repo data";
        send_response(client_fd, 500, "Internal Server Error", 
                     "text/html", msg, strlen(msg), 0);
        return;
    }
    
    struct MemoryStruct chunk = {malloc(1), 0};
    char url[512];
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s", config.github_repo);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    headers = curl_slist_append(headers, "User-Agent: 0xjah-website/1.0");
    
    if (strlen(config.github_token) > 0) {
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: token %s", 
                config.github_token);
        headers = curl_slist_append(headers, auth_header);
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        char *pushed_at = strstr(chunk.memory, "\"pushed_at\":\"");
        if (pushed_at) {
            pushed_at += 13;
            char *end = strchr(pushed_at, '"');
            if (end) {
                *end = '\0';
                time_t timestamp = parse_iso8601(pushed_at);
                char relative_time[64];
                get_relative_time(timestamp, relative_time, sizeof(relative_time));
                
                char response[256];
                snprintf(response, sizeof(response),
                    "<span title=\"%s\">%s</span>", pushed_at, relative_time);
                
                send_response(client_fd, 200, "OK", "text/html", 
                            response, strlen(response), 300);
            } else {
                const char *msg = "error parsing timestamp";
                send_response(client_fd, 500, "Internal Server Error", 
                             "text/html", msg, strlen(msg), 0);
            }
        } else {
            const char *msg = "error parsing repo data";
            send_response(client_fd, 500, "Internal Server Error", 
                         "text/html", msg, strlen(msg), 0);
        }
    } else {
        const char *msg = "error fetching repo data";
        send_response(client_fd, 500, "Internal Server Error", 
                     "text/html", msg, strlen(msg), 0);
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(chunk.memory);
}

/* Handle Discord status endpoint */
void handle_discord_status(int client_fd) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        char response[512];
        snprintf(response, sizeof(response),
            "<div class=\"status-header\"><h2>Status</h2>"
            "<p class=\"quote\">%s</p></div>", config.fallback_status);
        send_response(client_fd, 200, "OK", "text/html", response, strlen(response), 0);
        return;
    }
    
    struct MemoryStruct chunk = {malloc(1), 0};
    char url[256];
    snprintf(url, sizeof(url), "https://api.lanyard.rest/v1/users/%s", config.discord_id);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: 0xjah.xyz/1.0");
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    
    CURLcode res = curl_easy_perform(curl);
    
    char status_text[512];
    if (res == CURLE_OK && chunk.memory) {
        /* Parse status from JSON */
        char *status_ptr = strstr(chunk.memory, "\"discord_status\":\"");
        if (status_ptr) {
            status_ptr += 18;
            char *end = strchr(status_ptr, '"');
            if (end) {
                size_t len = end - status_ptr;
                if (len < sizeof(status_text)) {
                    strncpy(status_text, status_ptr, len);
                    status_text[len] = '\0';
                } else {
                    strcpy(status_text, config.fallback_status);
                }
            } else {
                strcpy(status_text, config.fallback_status);
            }
        } else {
            strcpy(status_text, config.fallback_status);
        }
    } else {
        strcpy(status_text, config.fallback_status);
    }
    
    char response[1024];
    snprintf(response, sizeof(response),
        "<div class=\"status-header\" hx-classes=\"add fade-in:200ms\">"
        "<h2>Status</h2>"
        "<p class=\"quote\" "
        "hx-on:click=\"htmx.trigger(this.parentElement, 'hx:refresh'); htmx.trigger('#repo-timestamp', 'refresh')\" "
        "style=\"cursor: pointer\" "
        "title=\"Click to refresh\">"
        "%s"
        "</p>"
        "<small style=\"opacity: 0.6; font-size: 0.75em; display: block; margin-top: 4px\">"
        "Last updated: "
        "<span id=\"repo-timestamp\" "
        "hx-get=\"/api/github-last-updated-html\" "
        "hx-trigger=\"load, every 5m, refresh\" "
        "hx-swap=\"innerHTML\" "
        "title=\"Last GitHub repository update\">"
        "loading..."
        "</span>"
        "</small>"
        "</div>",
        status_text);
    
    send_response(client_fd, 200, "OK", "text/html", response, strlen(response), 0);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(chunk.memory);
}

/* Handle gallery endpoint */
void handle_gallery(int client_fd) {
    if (strlen(config.r2_access_key) == 0 || strlen(config.r2_bucket) == 0) {
        const char *msg = "{\"error\": \"R2 not configured\"}";
        send_response(client_fd, 503, "Service Unavailable", 
                     "application/json", msg, strlen(msg), 0);
        return;
    }
    
    /* For now, return empty gallery - full S3 implementation would be complex */
    const char *msg = "{\"images\": [], \"count\": 0}";
    send_response(client_fd, 200, "OK", "application/json", msg, strlen(msg), 0);
}

/* Handle health check endpoint */
void handle_health_check(int client_fd) {
    char response[256];
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    
    snprintf(response, sizeof(response),
        "{\"status\":\"healthy\",\"timestamp\":\"%s\",\"environment\":\"%s\"}",
        timestamp, config.environment);
    
    send_response(client_fd, 200, "OK", "application/json", 
                 response, strlen(response), 0);
}

/* Handle last updated endpoint */
void handle_last_updated(int client_fd) {
    char response[256];
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    
    snprintf(response, sizeof(response), "{\"timestamp\":\"%s\"}", timestamp);
    send_response(client_fd, 200, "OK", "application/json", 
                 response, strlen(response), 0);
}

/* Parse HTTP request */
void parse_request(const char *request, char *method, char *path, char *query) {
    sscanf(request, "%s %s", method, path);
    
    char *q = strchr(path, '?');
    if (q) {
        strcpy(query, q + 1);
        *q = '\0';
    } else {
        query[0] = '\0';
    }
}

/* Handle client connection */
void* handle_client(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read <= 0) {
        close(client_fd);
        return NULL;
    }
    
    buffer[bytes_read] = '\0';
    
    char method[16], path[MAX_PATH], query[512];
    parse_request(buffer, method, path, query);
    
    printf("%s %s\n", method, path);
    
    /* Route handling */
    if (strcmp(path, "/health") == 0) {
        handle_health_check(client_fd);
    } else if (strcmp(path, "/api/last-updated") == 0) {
        handle_last_updated(client_fd);
    } else if (strcmp(path, "/api/github-last-updated-html") == 0) {
        handle_github_last_updated_html(client_fd);
    } else if (strcmp(path, "/api/discord-status") == 0) {
        handle_discord_status(client_fd);
    } else if (strcmp(path, "/api/gallery") == 0) {
        handle_gallery(client_fd);
    } else if (strcmp(path, "/") == 0) {
        serve_file(client_fd, "public/index.html");
    } else if (strcmp(path, "/blog") == 0) {
        if (strlen(query) > 0 && strstr(query, "post=")) {
            char post_file[MAX_PATH];
            char *post_name = strstr(query, "post=") + 5;
            char decoded_post[256];
            url_decode(decoded_post, post_name);
            snprintf(post_file, sizeof(post_file), "public/partials/blog/%s.html", decoded_post);
            serve_file(client_fd, post_file);
        } else {
            serve_file(client_fd, "public/blog.html");
        }
    } else if (strcmp(path, "/misc") == 0) {
        serve_file(client_fd, "public/misc.html");
    } else if (strcmp(path, "/gallery") == 0) {
        serve_file(client_fd, "public/gallery.html");
    } else if (strcmp(path, "/lain") == 0) {
        serve_file(client_fd, "public/lain.html");
    } else if (strncmp(path, "/static/", 8) == 0) {
        char file_path[MAX_PATH];
        snprintf(file_path, sizeof(file_path), "public%s", path);
        serve_file(client_fd, file_path);
    } else if (strncmp(path, "/partials/", 10) == 0) {
        char file_path[MAX_PATH];
        snprintf(file_path, sizeof(file_path), "public%s", path);
        serve_file(client_fd, file_path);
    } else {
        const char *msg = "404 Not Found";
        send_response(client_fd, 404, "Not Found", "text/plain", msg, strlen(msg), 0);
    }
    
    close(client_fd);
    return NULL;
}

/* Signal handler for graceful shutdown */
void signal_handler(int sig) {
    printf("\nShutting down server...\n");
    running = 0;
    if (server_fd >= 0) {
        close(server_fd);
    }
}

int main() {
    struct sockaddr_in address;
    int opt = 1;
    
    /* Load configuration */
    load_config();
    
    /* Initialize CURL */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create socket */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    /* Set socket options - macOS compatible */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(config.port));
    
    /* Bind socket */
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    /* Listen for connections */
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Server starting on http://%s:%s\n", config.host, config.port);
    printf("Environment: %s\n", config.environment);
    printf("Server started successfully\n");
    
    /* Accept and handle connections */
    while (running) {
        int *client_fd = malloc(sizeof(int));
        socklen_t addrlen = sizeof(address);
        
        *client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        
        if (*client_fd < 0) {
            if (running) perror("accept");
            free(client_fd);
            continue;
        }
        
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, client_fd);
        pthread_detach(thread_id);
    }
    
    /* Cleanup */
    curl_global_cleanup();
    printf("Server shutdown complete\n");
    
    return 0;
}
