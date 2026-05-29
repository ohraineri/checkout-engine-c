#ifndef ENGINE_TEMPLATE_CACHE_H
#define ENGINE_TEMPLATE_CACHE_H

#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include "engine/theme_loader.h"
#include "engine/tpl_engine.h"

#define CACHE_MAX_ENTRIES 256

typedef struct {
    char    key[256];     /* "themeSlug:step" */
    TplSet *tplset;
} CacheEntry;

typedef struct {
    pthread_rwlock_t lock;
    CacheEntry       entries[CACHE_MAX_ENTRIES];
    int              count;
    ThemeLoader     *loader;
} TemplateCache;

TemplateCache *template_cache_new(ThemeLoader *loader);
void           template_cache_free(TemplateCache *tc);

/* Returns TplSet* (owned by cache). Returns NULL on error. */
TplSet *template_cache_get(TemplateCache *tc,
                            const char *theme_slug,
                            const char *step);

/* Precompile all steps for a theme */
int template_cache_warmup(TemplateCache *tc,
                           const char *theme_slug,
                           const char *steps[], int nsteps);

#endif /* ENGINE_TEMPLATE_CACHE_H */
