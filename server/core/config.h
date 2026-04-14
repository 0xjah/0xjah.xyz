#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

typedef struct {
    char port[6];
    char github_repo[256];
    char r2_endpoint[512];
    char r2_access_key[256];
    char r2_secret_key[256];
    char r2_bucket[256];
    char r2_public_url[512];
    char content_dir[256];
} Config;

void config_load(void);
const Config *config_get(void);

#endif
