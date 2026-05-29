#ifndef ENGINE_THEME_LOADER_H
#define ENGINE_THEME_LOADER_H

#include <stddef.h>

#define MAX_THEMES      64
#define MAX_THEME_SLUG  128
#define MAX_FILES       256

typedef struct {
    int  ttl_seconds;
    char vary_by[8][64];
    int  vary_by_count;
} ThemeCacheConfig;

typedef struct {
    char css[16][128];
    int  css_count;
    char js[16][128];
    int  js_count;
    int  fingerprint;
} ThemeAssetsConfig;

typedef struct {
    char              name[128];
    char              version[32];
    char              inherits[MAX_THEME_SLUG];
    char              flow_profile[64];
    int               features_count;
    char              feature_keys[32][128];
    int               feature_vals[32];
    ThemeCacheConfig  cache;
    ThemeAssetsConfig assets;
} ThemeManifest;

typedef struct {
    char          slug[MAX_THEME_SLUG];
    ThemeManifest manifest;
} ThemeEntry;

typedef struct {
    char        themes_root[512];
    ThemeEntry  entries[MAX_THEMES];
    int         count;
} ThemeLoader;

ThemeLoader *theme_loader_new(const char *themes_root);
void         theme_loader_free(ThemeLoader *tl);

/* Returns 0 on success */
int  theme_loader_load_all(ThemeLoader *tl);

/* Sorted list of theme slugs; slugs array must have room for MAX_THEMES entries */
int  theme_loader_themes(const ThemeLoader *tl, char slugs[][MAX_THEME_SLUG], int max);

/* Fills files[] with resolved file paths; returns count, -1 on error */
int  theme_loader_resolve_files(const ThemeLoader *tl,
                                const char *theme_slug,
                                const char *step,
                                char files[][512], int max_files);

/* Lookup manifest; returns NULL if not found */
const ThemeManifest *theme_loader_get_manifest(const ThemeLoader *tl, const char *slug);

/* Build inheritance chain from most-base to most-derived.
 * chain[][MAX_THEME_SLUG], returns chain length or -1 on error */
int  theme_loader_build_chain(const ThemeLoader *tl,
                              const char *slug,
                              char chain[][MAX_THEME_SLUG], int maxchain);

#endif /* ENGINE_THEME_LOADER_H */
