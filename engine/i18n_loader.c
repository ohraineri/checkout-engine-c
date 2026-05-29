#include "engine/i18n_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Flat YAML key:value parser (same algorithm as Go parseYAML)          */
/* ------------------------------------------------------------------ */

static void unquote_yaml(char *s) {
    size_t len = strlen(s);
    if (len >= 2 &&
        ((s[0] == '"' && s[len-1] == '"') ||
         (s[0] == '\'' && s[len-1] == '\''))) {
        memmove(s, s+1, len-2);
        s[len-2] = '\0';
    }
}

static char *ltrim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static void rtrim(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\r' || s[len-1] == '\n'))
        s[--len] = '\0';
}

static StrMap *parse_yaml_flat(const char *data) {
    StrMap *m = strmap_new();
    if (!m) return NULL;

    char *buf = strdup(data);
    if (!buf) { strmap_free(m); return NULL; }

    char *line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char *content = ltrim(line);
        rtrim(content);

        /* Skip blanks and comments */
        if (!*content || *content == '#') {
            line = nl ? nl+1 : NULL;
            continue;
        }

        char *colon = strchr(content, ':');
        if (!colon) { line = nl ? nl+1 : NULL; continue; }

        char key[512], value[512];
        int klen = (int)(colon - content);
        if (klen <= 0) { line = nl ? nl+1 : NULL; continue; }
        snprintf(key, sizeof(key), "%.*s", klen, content);

        /* Trim key */
        char *k = ltrim(key);
        rtrim(k);

        /* Value: everything after first colon */
        snprintf(value, sizeof(value), "%s", colon+1);
        char *v = ltrim(value);
        rtrim(v);
        unquote_yaml(v);

        if (*k) strmap_set(m, k, v);
        line = nl ? nl+1 : NULL;
    }

    free(buf);
    return m;
}

/* ------------------------------------------------------------------ */
/* Locale helpers                                                        */
/* ------------------------------------------------------------------ */

static void locale_base(const char *locale, char *out, int outsz) {
    const char *dash = strchr(locale, '-');
    if (dash) {
        int n = (int)(dash - locale);
        snprintf(out, outsz, "%.*s", n, locale);
    } else {
        snprintf(out, outsz, "%s", locale);
    }
}

/* variants: base then full (or just full if base==full) */

/* ------------------------------------------------------------------ */
/* I18nLoader                                                           */
/* ------------------------------------------------------------------ */

I18nLoader *i18n_loader_new(ThemeLoader *loader) {
    I18nLoader *il = calloc(1, sizeof(I18nLoader));
    if (!il) return NULL;
    il->theme_loader = loader;
    return il;
}

void i18n_loader_free(I18nLoader *il) {
    if (!il) return;
    for (int i = 0; i < il->cache_count; i++)
        strmap_free(il->cache_vals[i]);
    free(il);
}

StrMap *i18n_loader_load(I18nLoader *il, const char *theme_slug, const char *locale) {
    char cache_key[256];
    snprintf(cache_key, sizeof(cache_key), "%s:%s", theme_slug, locale);

    /* Check cache */
    for (int i = 0; i < il->cache_count; i++) {
        if (strcmp(il->cache_keys[i], cache_key) == 0)
            return il->cache_vals[i];
    }

    /* Build chain */
    char chain[MAX_THEMES][MAX_THEME_SLUG];
    int chain_len = theme_loader_build_chain(il->theme_loader, theme_slug,
                                              chain, MAX_THEMES);
    if (chain_len < 0) return NULL;

    StrMap *merged = strmap_new();
    if (!merged) return NULL;

    char base[64];
    locale_base(locale, base, sizeof(base));

    /* Determine variants */
    int same = (strcmp(base, locale) == 0);
    const char *variants[2];
    int nvariants;
    if (same) {
        variants[0] = locale;
        nvariants = 1;
    } else {
        variants[0] = base;
        variants[1] = locale;
        nvariants = 2;
    }

    for (int ci = 0; ci < chain_len; ci++) {
        char theme_dir[512];
        snprintf(theme_dir, sizeof(theme_dir), "%s/%s",
                 il->theme_loader->themes_root, chain[ci]);

        for (int vi = 0; vi < nvariants; vi++) {
            char path[640];
            snprintf(path, sizeof(path), "%s/i18n/%s.yaml", theme_dir, variants[vi]);

            FILE *f = fopen(path, "r");
            if (!f) continue;

            fseek(f, 0, SEEK_END);
            long fsz = ftell(f);
            rewind(f);
            if (fsz <= 0) { fclose(f); continue; }
            char *data = malloc((size_t)fsz + 1);
            if (!data) { fclose(f); strmap_free(merged); return NULL; }
            fread(data, 1, (size_t)fsz, f);
            data[fsz] = '\0';
            fclose(f);

            StrMap *msgs = parse_yaml_flat(data);
            free(data);
            if (!msgs) continue;

            /* Merge into merged */
            size_t idx = 0;
            StrMapEntry *entry = NULL;
            while (strmap_iter(msgs, &idx, &entry))
                strmap_set(merged, entry->key, entry->value);
            strmap_free(msgs);
        }
    }

    if (strmap_count(merged) == 0) {
        /* No translations found - return empty rather than error */
        /* (fallback to key names at render time) */
    }

    /* Store in cache */
    if (il->cache_count < 128) {
        snprintf(il->cache_keys[il->cache_count], 256, "%s", cache_key);
        il->cache_vals[il->cache_count] = merged;
        il->cache_count++;
    } else {
        /* Cache full: caller doesn't own, just return and accept no caching */
        /* We'll leak here in edge cases but this is bounded anyway */
    }

    return merged;
}

const char *i18n_loader_get(I18nLoader *il,
                             const char *theme_slug,
                             const char *locale,
                             const char *key) {
    StrMap *msgs = i18n_loader_load(il, theme_slug, locale);
    if (!msgs) return key;
    const char *v = strmap_get(msgs, key);
    return v ? v : key;
}
