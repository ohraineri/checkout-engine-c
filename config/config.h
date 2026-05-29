#ifndef CONFIG_CONFIG_H
#define CONFIG_CONFIG_H

typedef struct {
    char host[128];
    char port[16];
    int  read_timeout;   /* seconds */
    int  write_timeout;
    int  idle_timeout;
} ServerConfig;

typedef struct {
    char root_path[512];
} ThemesConfig;

typedef struct {
    int  shell_max_age;
    int  warm_up_on_boot;
} CacheConfig;

typedef struct {
    char base_url[512];
} CDNConfig;

typedef struct {
    char locale[64];
    char currency[16];
} DefaultsConfig;

typedef struct {
    ServerConfig   server;
    ThemesConfig   themes;
    CacheConfig    cache;
    CDNConfig      cdn;
    DefaultsConfig defaults;
} Config;

void config_load(Config *cfg);

/* Returns "host:port" in out (must be at least 148 bytes) */
void config_addr(const Config *cfg, char *out, int outsz);

#endif /* CONFIG_CONFIG_H */
