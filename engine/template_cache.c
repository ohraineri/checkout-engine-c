#include "engine/template_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TemplateCache *template_cache_new(ThemeLoader *loader) {
    TemplateCache *tc = calloc(1, sizeof(TemplateCache));
    if (!tc) return NULL;
    pthread_rwlock_init(&tc->lock, NULL);
    tc->loader = loader;
    return tc;
}

void template_cache_free(TemplateCache *tc) {
    if (!tc) return;
    for (int i = 0; i < tc->count; i++)
        tpl_set_free(tc->entries[i].tplset);
    pthread_rwlock_destroy(&tc->lock);
    free(tc);
}

static TplSet *compile(ThemeLoader *loader,
                        const char *theme_slug,
                        const char *step) {
    char files[MAX_FILES][512];
    int nfiles = theme_loader_resolve_files(loader, theme_slug, step,
                                            files, MAX_FILES);
    if (nfiles < 0) {
        fprintf(stderr, "Failed to resolve files for %s/%s\n", theme_slug, step);
        return NULL;
    }
    if (nfiles == 0) {
        fprintf(stderr, "No template files for %s/%s\n", theme_slug, step);
        return NULL;
    }

    const char *ptrs[MAX_FILES];
    for (int i = 0; i < nfiles; i++)
        ptrs[i] = files[i];

    TplSet *ts = tpl_parse_files(ptrs, nfiles);
    return ts;
}

TplSet *template_cache_get(TemplateCache *tc,
                            const char *theme_slug,
                            const char *step) {
    char key[256];
    snprintf(key, sizeof(key), "%s:%s", theme_slug, step);

    /* Read lock */
    pthread_rwlock_rdlock(&tc->lock);
    for (int i = 0; i < tc->count; i++) {
        if (strcmp(tc->entries[i].key, key) == 0) {
            TplSet *ts = tc->entries[i].tplset;
            pthread_rwlock_unlock(&tc->lock);
            return ts;
        }
    }
    pthread_rwlock_unlock(&tc->lock);

    /* Compile */
    TplSet *ts = compile(tc->loader, theme_slug, step);
    if (!ts) return NULL;

    /* Write lock */
    pthread_rwlock_wrlock(&tc->lock);
    /* Check again */
    for (int i = 0; i < tc->count; i++) {
        if (strcmp(tc->entries[i].key, key) == 0) {
            pthread_rwlock_unlock(&tc->lock);
            tpl_set_free(ts);
            return tc->entries[i].tplset;
        }
    }
    if (tc->count < CACHE_MAX_ENTRIES) {
        snprintf(tc->entries[tc->count].key, 256, "%s", key);
        tc->entries[tc->count].tplset = ts;
        tc->count++;
    }
    pthread_rwlock_unlock(&tc->lock);
    return ts;
}

int template_cache_warmup(TemplateCache *tc,
                           const char *theme_slug,
                           const char *steps[], int nsteps) {
    for (int i = 0; i < nsteps; i++) {
        TplSet *ts = template_cache_get(tc, theme_slug, steps[i]);
        if (!ts) {
            fprintf(stderr, "Warm-up failed: %s/%s\n", theme_slug, steps[i]);
            /* Non-fatal: continue */
        }
    }
    return 0;
}
