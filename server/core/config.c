#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Config g_config;

void config_load(void) {
    FILE *fp = fopen(".env", "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (line[0] == '#' || line[0] == '\n') {
                continue;
            }

            line[strcspn(line, "\n\r")] = '\0';
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                setenv(line, eq + 1, 0);
            }
        }
        fclose(fp);
    }

    const char *env;
#define LOAD_ENV(field, name, def) \
    snprintf(g_config.field, sizeof(g_config.field), "%s", (env = getenv(name)) ? env : def)

    LOAD_ENV(port, "PORT", "3000");
    LOAD_ENV(github_repo, "GITHUB_REPO", "0xjah/0xjah.xyz");
    LOAD_ENV(r2_endpoint, "R2_ENDPOINT", "");
    LOAD_ENV(r2_access_key, "R2_ACCESS_KEY", "");
    LOAD_ENV(r2_secret_key, "R2_SECRET_KEY", "");
    LOAD_ENV(r2_bucket, "R2_BUCKET", "");
    LOAD_ENV(r2_public_url, "R2_PUBLIC_URL", "");
    LOAD_ENV(content_dir, "CONTENT_DIR", "content");

#undef LOAD_ENV

    printf("[Config] Server port: %s\n", g_config.port);
    printf("[Config] Content dir: %s\n", g_config.content_dir);
}

const Config *config_get(void) {
    return &g_config;
}
