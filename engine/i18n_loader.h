#ifndef ENGINE_I18N_LOADER_H
#define ENGINE_I18N_LOADER_H

#include "engine/theme_loader.h"
#include "util/strmap.h"

typedef struct {
    ThemeLoader *theme_loader;
    /* Cache: key = "slug:locale", value = StrMap* cast to void* */
    /* We store up to 128 entries */
    char       cache_keys[128][256];
    StrMap    *cache_vals[128];
    int        cache_count;
} I18nLoader;

I18nLoader *i18n_loader_new(ThemeLoader *loader);
void        i18n_loader_free(I18nLoader *il);

/* Returns StrMap* (caller does NOT free - owned by cache).
 * Returns NULL on error. */
StrMap *i18n_loader_load(I18nLoader *il, const char *theme_slug, const char *locale);

const char *i18n_loader_get(I18nLoader *il,
                             const char *theme_slug,
                             const char *locale,
                             const char *key);

#endif /* ENGINE_I18N_LOADER_H */
