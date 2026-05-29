#include "config/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *env_or(const char *key, const char *def) {
    const char *v = getenv(key);
    return (v && v[0]) ? v : def;
}

static int env_int_or(const char *key, int def) {
    const char *v = getenv(key);
    if (v && v[0]) return atoi(v);
    return def;
}

static int env_bool_or(const char *key, int def) {
    const char *v = getenv(key);
    if (!v || !v[0]) return def;
    if (strcmp(v, "true") == 0  || strcmp(v, "1") == 0) return 1;
    if (strcmp(v, "false") == 0 || strcmp(v, "0") == 0) return 0;
    return def;
}

void config_load(Config *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    snprintf(cfg->server.host,  sizeof(cfg->server.host),  "%s", env_or("SERVER_HOST", "0.0.0.0"));
    snprintf(cfg->server.port,  sizeof(cfg->server.port),  "%s", env_or("SERVER_PORT", "8080"));
    cfg->server.read_timeout  = env_int_or("SERVER_READ_TIMEOUT",  5);
    cfg->server.write_timeout = env_int_or("SERVER_WRITE_TIMEOUT", 10);
    cfg->server.idle_timeout  = env_int_or("SERVER_IDLE_TIMEOUT",  60);

    snprintf(cfg->themes.root_path, sizeof(cfg->themes.root_path), "%s",
             env_or("THEMES_ROOT_PATH", "./themes"));

    cfg->cache.shell_max_age  = env_int_or("CACHE_SHELL_MAX_AGE",   300);
    cfg->cache.warm_up_on_boot = env_bool_or("CACHE_WARM_UP_ON_BOOT", 1);

    snprintf(cfg->cdn.base_url, sizeof(cfg->cdn.base_url), "%s",
             env_or("CDN_BASE_URL", ""));

    snprintf(cfg->defaults.locale,   sizeof(cfg->defaults.locale),   "%s",
             env_or("DEFAULT_LOCALE",   "pt-BR"));
    snprintf(cfg->defaults.currency, sizeof(cfg->defaults.currency), "%s",
             env_or("DEFAULT_CURRENCY", "BRL"));
}

void config_addr(const Config *cfg, char *out, int outsz) {
    snprintf(out, outsz, "%s:%s", cfg->server.host, cfg->server.port);
}
