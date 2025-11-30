/*
 * Build: gcc -o server main.c -lcurl -lpthread -lssl -lcrypto -O3 -Wall
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
#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#define BUFFER_SIZE 16384
#define MAX_PATH 512

typedef struct {
    char port[6];
    char github_repo[256];
    char r2_endpoint[512];
    char r2_access_key[256];
    char r2_secret_key[256];
    char r2_bucket[256];
} Config;

Config cfg;
int server_fd;
volatile sig_atomic_t running = 1;

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
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

void load_config() {
    char *e;
    snprintf(cfg.port, sizeof(cfg.port), "%s", (e = getenv("PORT")) ? e : "3000");
    snprintf(cfg.github_repo, sizeof(cfg.github_repo), "%s", (e = getenv("GITHUB_REPO")) ? e : "0xjah/0xjah.xyz");
    snprintf(cfg.r2_endpoint, sizeof(cfg.r2_endpoint), "%s", (e = getenv("R2_ENDPOINT")) ? e : "");
    snprintf(cfg.r2_access_key, sizeof(cfg.r2_access_key), "%s", (e = getenv("R2_ACCESS_KEY")) ? e : "");
    snprintf(cfg.r2_secret_key, sizeof(cfg.r2_secret_key), "%s", (e = getenv("R2_SECRET_KEY")) ? e : "");
    snprintf(cfg.r2_bucket, sizeof(cfg.r2_bucket), "%s", (e = getenv("R2_BUCKET")) ? e : "");
    printf("Server on port %s\n", cfg.port);
}

void send_response(int fd, int code, const char *type, const char *body, size_t len) {
    char header[2048];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Connection: keep-alive\r\nAccess-Control-Allow-Origin: *\r\n\r\n",
        code, type, len);
    write(fd, header, hlen);
    if (body && len > 0) write(fd, body, len);
}

const char* get_mime(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "text/plain";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
    if (strcmp(ext, ".webp") == 0) return "image/webp";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    return "application/octet-stream";
}

void serve_file(int fd, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        const char *msg = "404 Not Found";
        send_response(fd, 404, "text/plain", msg, strlen(msg));
        return;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *content = malloc(size);
    if (!content) {
        fclose(fp);
        const char *msg = "500 Error";
        send_response(fd, 500, "text/plain", msg, strlen(msg));
        return;
    }
    fread(content, 1, size, fp);
    fclose(fp);
    send_response(fd, 200, get_mime(path), content, size);
    free(content);
}

void hmac_sha256(const unsigned char *key, int keylen, const unsigned char *data, 
                 int datalen, unsigned char *out) {
    unsigned int len;
    HMAC(EVP_sha256(), key, keylen, data, datalen, out, &len);
}

void sha256_hex(const char *data, char *out) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data, strlen(data), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(out + (i * 2), "%02x", hash[i]);
}

void create_aws_signature(const char *method, const char *uri, const char *query,
                          const char *date, const char *datetime, char *auth_header) {
    char canonical_request[4096];
    snprintf(canonical_request, sizeof(canonical_request),
        "%s\n%s\n%s\nhost:%s\nx-amz-content-sha256:UNSIGNED-PAYLOAD\nx-amz-date:%s\n\n"
        "host;x-amz-content-sha256;x-amz-date\nUNSIGNED-PAYLOAD",
        method, uri, query ? query : "", cfg.r2_endpoint, datetime);
    
    char canonical_hash[65];
    sha256_hex(canonical_request, canonical_hash);
    
    char string_to_sign[1024];
    snprintf(string_to_sign, sizeof(string_to_sign),
        "AWS4-HMAC-SHA256\n%s\n%s/auto/s3/aws4_request\n%s",
        datetime, date, canonical_hash);
    
    unsigned char k_date[32], k_region[32], k_service[32], k_signing[32];
    char key[256];
    snprintf(key, sizeof(key), "AWS4%s", cfg.r2_secret_key);
    
    hmac_sha256((unsigned char*)key, strlen(key), (unsigned char*)date, strlen(date), k_date);
    hmac_sha256(k_date, 32, (unsigned char*)"auto", 4, k_region);
    hmac_sha256(k_region, 32, (unsigned char*)"s3", 2, k_service);
    hmac_sha256(k_service, 32, (unsigned char*)"aws4_request", 12, k_signing);
    
    unsigned char signature[32];
    hmac_sha256(k_signing, 32, (unsigned char*)string_to_sign, strlen(string_to_sign), signature);
    
    char sig_hex[65];
    for (int i = 0; i < 32; i++)
        sprintf(sig_hex + (i * 2), "%02x", signature[i]);
    
    snprintf(auth_header, 512,
        "AWS4-HMAC-SHA256 Credential=%s/%s/auto/s3/aws4_request,"
        "SignedHeaders=host;x-amz-content-sha256;x-amz-date,Signature=%s",
        cfg.r2_access_key, date, sig_hex);
}

void handle_gallery(int fd) {
    if (strlen(cfg.r2_access_key) == 0) {
        const char *msg = "{\"images\":[]}";
        send_response(fd, 200, "application/json", msg, strlen(msg));
        return;
    }
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        const char *msg = "{\"images\":[]}";
        send_response(fd, 500, "application/json", msg, strlen(msg));
        return;
    }
    
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    char date[9], datetime[17];
    strftime(date, sizeof(date), "%Y%m%d", tm_info);
    strftime(datetime, sizeof(datetime), "%Y%m%dT%H%M%SZ", tm_info);
    
    char auth_header[512];
    create_aws_signature("GET", "/", "list-type=2", date, datetime, auth_header);
    
    char url[1024];
    snprintf(url, sizeof(url), "https://%s/%s?list-type=2", cfg.r2_endpoint, cfg.r2_bucket);
    
    struct curl_slist *headers = NULL;
    char auth_h[600], date_h[100];
    snprintf(auth_h, sizeof(auth_h), "Authorization: %s", auth_header);
    snprintf(date_h, sizeof(date_h), "x-amz-date: %s", datetime);
    headers = curl_slist_append(headers, auth_h);
    headers = curl_slist_append(headers, date_h);
    headers = curl_slist_append(headers, "x-amz-content-sha256: UNSIGNED-PAYLOAD");
    
    struct MemoryStruct chunk = {malloc(1), 0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK && chunk.memory) {
        char response[16384] = "{\"images\":[";
        int count = 0;
        char *key_start = chunk.memory;
        
        while ((key_start = strstr(key_start, "<Key>")) != NULL) {
            key_start += 5;
            char *key_end = strstr(key_start, "</Key>");
            if (!key_end) break;
            
            size_t key_len = key_end - key_start;
            char key[256];
            strncpy(key, key_start, key_len);
            key[key_len] = '\0';
            
            if (!strstr(key, ".jpg") && !strstr(key, ".png") && 
                !strstr(key, ".webp") && !strstr(key, ".jpeg")) {
                key_start = key_end;
                continue;
            }
            
            if (count > 0) strcat(response, ",");
            char img_entry[512];
            snprintf(img_entry, sizeof(img_entry),
                "{\"key\":\"%s\",\"url\":\"https://%s/%s/%s\"}",
                key, cfg.r2_endpoint, cfg.r2_bucket, key);
            strcat(response, img_entry);
            count++;
            key_start = key_end;
        }
        
        strcat(response, "],\"count\":");
        char count_str[16];
        snprintf(count_str, sizeof(count_str), "%d}", count);
        strcat(response, count_str);
        
        send_response(fd, 200, "application/json", response, strlen(response));
    } else {
        const char *msg = "{\"images\":[]}";
        send_response(fd, 500, "application/json", msg, strlen(msg));
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(chunk.memory);
}

void handle_github_status(int fd) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        const char *msg = "<div><h2>Status</h2><p>Last updated: unknown</p></div>";
        send_response(fd, 200, "text/html", msg, strlen(msg));
        return;
    }
    
    struct MemoryStruct chunk = {malloc(1), 0};
    char url[512];
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s", cfg.github_repo);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: 0xjah.xyz/1.0");
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    
    CURLcode res = curl_easy_perform(curl);
    
    char time_str[64] = "unknown";
    if (res == CURLE_OK && chunk.memory) {
        char *pushed = strstr(chunk.memory, "\"pushed_at\":\"");
        if (pushed) {
            pushed += 13;
            char *end = strchr(pushed, '"');
            if (end && (end - pushed) < sizeof(time_str)) {
                strncpy(time_str, pushed, end - pushed);
                time_str[end - pushed] = '\0';
            }
        }
    }
    
    char response[512];
    snprintf(response, sizeof(response),
        "<div class=\"status-header\"><h2>Status</h2>"
        "<p class=\"quote\">Last updated: %s</p></div>", time_str);
    
    send_response(fd, 200, "text/html", response, strlen(response));
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(chunk.memory);
}

void* handle_client(void *arg) {
    int fd = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    int n = read(fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        close(fd);
        return NULL;
    }
    buffer[n] = '\0';
    
    char method[16], path[MAX_PATH];
    sscanf(buffer, "%s %s", method, path);
    char *q = strchr(path, '?');
    if (q) *q = '\0';
    
    printf("%s %s\n", method, path);
    
    if (strcmp(path, "/api/gallery") == 0) {
        handle_gallery(fd);
    } else if (strcmp(path, "/api/github-status") == 0) {
        handle_github_status(fd);
    } else if (strcmp(path, "/health") == 0) {
        const char *msg = "{\"status\":\"ok\"}";
        send_response(fd, 200, "application/json", msg, strlen(msg));
    } else if (strcmp(path, "/") == 0) {
        serve_file(fd, "public/index.html");
    } else if (strncmp(path, "/static/", 8) == 0 || strncmp(path, "/partials/", 10) == 0) {
        char file[MAX_PATH];
        snprintf(file, sizeof(file), "public%s", path);
        serve_file(fd, file);
    } else {
        const char *msg = "404";
        send_response(fd, 404, "text/plain", msg, strlen(msg));
    }
    
    close(fd);
    return NULL;
}

void signal_handler(int sig) {
    running = 0;
    if (server_fd >= 0) close(server_fd);
}

int main() {
    load_config();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    struct sockaddr_in addr;
    int opt = 1;
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        exit(1);
    }
    
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(atoi(cfg.port));
    
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }
    
    if (listen(server_fd, 1000) < 0) {
        perror("listen");
        exit(1);
    }
    
    printf("Server running on http://0.0.0.0:%s\n", cfg.port);
    
    while (running) {
        int *client_fd = malloc(sizeof(int));
        socklen_t addrlen = sizeof(addr);
        *client_fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
        
        if (*client_fd < 0) {
            free(client_fd);
            continue;
        }
        
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }
    
    curl_global_cleanup();
    return 0;
}