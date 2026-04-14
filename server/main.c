
#include "core/config.h"
#include "core/router.h"

#include <curl/curl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

static int server_fd = -1;
static volatile sig_atomic_t running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
    if (server_fd >= 0) {
        close(server_fd);
    }
}

int main(void) {
    config_load();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    const Config *cfg = config_get();

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons((unsigned short)atoi(cfg->port))
    };

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 1000) < 0) {
        perror("listen");
        return 1;
    }

    printf("\n");
    printf("  0xjah.xyz server\n");
    printf("  http://0.0.0.0:%s\n", cfg->port);
    printf("\n");

    while (running) {
        socklen_t addrlen = sizeof(addr);
        int *client_fd = malloc(sizeof(int));
        if (!client_fd) {
            continue;
        }

        *client_fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
        if (*client_fd < 0) {
            free(client_fd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, router_handle_client, client_fd);
        pthread_detach(tid);
    }

    curl_global_cleanup();
    printf("\nServer stopped.\n");
    return 0;
}
