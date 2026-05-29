#include "engine/theme_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ */
/* YAML manifest parser                                                 */
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

static int count_indent(const char *line) {
    int i = 0;
    while (line[i] == ' ' || line[i] == '\t') i++;
    return i;
}

static char *trim_space(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\r' || s[len-1] == '\n'))
        s[--len] = '\0';
    return s;
}

static int parse_manifest(const char *data, ThemeManifest *m) {
    memset(m, 0, sizeof(*m));

    char *buf = strdup(data);
    if (!buf) return -1;

    char section[64] = "";
    char subkey[64]  = "";

    char *line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        /* normalize \r\n */
        size_t ll = strlen(line);
        if (ll > 0 && line[ll-1] == '\r') line[ll-1] = '\0';

        int indent = count_indent(line);
        char *content = trim_space(line);

        if (!*content) { line = nl ? nl+1 : NULL; continue; }

        /* list item */
        if (indent >= 4 && strncmp(content, "- ", 2) == 0) {
            char item[128];
            snprintf(item, sizeof(item), "%s", content + 2);
            unquote_yaml(item);

            if (strcmp(section, "cache") == 0 && strcmp(subkey, "vary_by") == 0) {
                if (m->cache.vary_by_count < 8) {
                    snprintf(m->cache.vary_by[m->cache.vary_by_count++],
                             64, "%s", item);
                }
            } else if (strcmp(section, "assets") == 0) {
                if (strcmp(subkey, "css") == 0 && m->assets.css_count < 16) {
                    snprintf(m->assets.css[m->assets.css_count++], 128, "%s", item);
                } else if (strcmp(subkey, "js") == 0 && m->assets.js_count < 16) {
                    snprintf(m->assets.js[m->assets.js_count++], 128, "%s", item);
                }
            }
            line = nl ? nl+1 : NULL;
            continue;
        }

        char *colon = strchr(content, ':');
        if (!colon) { line = nl ? nl+1 : NULL; continue; }

        char key[128], value[256];
        int klen = (int)(colon - content);
        if (klen <= 0 || klen >= 128) { line = nl ? nl+1 : NULL; continue; }
        snprintf(key, sizeof(key), "%.*s", klen, content);
        char *k = trim_space(key);
        snprintf(value, sizeof(value), "%s", colon+1);
        char *v = trim_space(value);
        unquote_yaml(v);

        switch (indent) {
        case 0:
            section[0] = '\0'; subkey[0] = '\0';
            if (!v[0]) {
                snprintf(section, sizeof(section), "%s", k);
            } else {
                if (strcmp(k, "name")         == 0) snprintf(m->name,        sizeof(m->name),        "%s", v);
                else if (strcmp(k, "version")  == 0) snprintf(m->version,     sizeof(m->version),     "%s", v);
                else if (strcmp(k, "inherits") == 0) snprintf(m->inherits,    sizeof(m->inherits),    "%s", v);
                else if (strcmp(k, "flow_profile") == 0) snprintf(m->flow_profile, sizeof(m->flow_profile), "%s", v);
            }
            break;
        case 2:
            subkey[0] = '\0';
            if (!v[0]) {
                snprintf(subkey, sizeof(subkey), "%s", k);
            } else {
                if (strcmp(section, "features") == 0) {
                    if (m->features_count < 32) {
                        snprintf(m->feature_keys[m->features_count], 128, "%s", k);
                        m->feature_vals[m->features_count] = (strcmp(v, "true") == 0);
                        m->features_count++;
                    }
                } else if (strcmp(section, "cache") == 0) {
                    if (strcmp(k, "ttl_seconds") == 0)
                        m->cache.ttl_seconds = atoi(v);
                } else if (strcmp(section, "assets") == 0) {
                    if (strcmp(k, "fingerprint") == 0)
                        m->assets.fingerprint = (strcmp(v, "true") == 0);
                }
            }
            break;
        default:
            /* deeper nesting: update subkey */
            if (indent >= 2 && !v[0]) {
                snprintf(subkey, sizeof(subkey), "%s", k);
            }
            break;
        }

        line = nl ? nl+1 : NULL;
    }

    free(buf);
    return 0;
}

/* ------------------------------------------------------------------ */
/* ThemeLoader                                                          */
/* ------------------------------------------------------------------ */

ThemeLoader *theme_loader_new(const char *themes_root) {
    ThemeLoader *tl = calloc(1, sizeof(ThemeLoader));
    if (!tl) return NULL;
    snprintf(tl->themes_root, sizeof(tl->themes_root), "%s", themes_root);
    return tl;
}

void theme_loader_free(ThemeLoader *tl) {
    free(tl);
}

int theme_loader_load_all(ThemeLoader *tl) {
    DIR *d = opendir(tl->themes_root);
    if (!d) { perror("opendir themes"); return -1; }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (strcmp(de->d_name, "_base") == 0) continue;

        /* Check it's a directory */
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", tl->themes_root, de->d_name);
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        /* Read theme.yaml */
        char yaml_path[640];
        snprintf(yaml_path, sizeof(yaml_path), "%s/theme.yaml", path);
        FILE *f = fopen(yaml_path, "r");
        if (!f) {
            fprintf(stderr, "Warning: cannot open %s\n", yaml_path);
            continue;
        }
        fseek(f, 0, SEEK_END);
        long fsz = ftell(f);
        rewind(f);
        char *data = malloc((size_t)fsz + 1);
        if (!data) { fclose(f); closedir(d); return -1; }
        fread(data, 1, (size_t)fsz, f);
        data[fsz] = '\0';
        fclose(f);

        if (tl->count >= MAX_THEMES) {
            fprintf(stderr, "Too many themes\n");
            free(data);
            break;
        }
        ThemeEntry *entry = &tl->entries[tl->count];
        snprintf(entry->slug, sizeof(entry->slug), "%s", de->d_name);
        if (parse_manifest(data, &entry->manifest) != 0) {
            fprintf(stderr, "Failed to parse manifest for theme %s\n", de->d_name);
            free(data);
            closedir(d);
            return -1;
        }
        free(data);
        tl->count++;
    }
    closedir(d);
    return 0;
}

static int slug_cmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int theme_loader_themes(const ThemeLoader *tl, char slugs[][MAX_THEME_SLUG], int max) {
    int n = tl->count < max ? tl->count : max;
    for (int i = 0; i < n; i++)
        snprintf(slugs[i], MAX_THEME_SLUG, "%s", tl->entries[i].slug);
    /* Sort */
    qsort(slugs, (size_t)n, MAX_THEME_SLUG, slug_cmp);
    return n;
}

const ThemeManifest *theme_loader_get_manifest(const ThemeLoader *tl, const char *slug) {
    for (int i = 0; i < tl->count; i++) {
        if (strcmp(tl->entries[i].slug, slug) == 0)
            return &tl->entries[i].manifest;
    }
    return NULL;
}

/* Returns chain length (base -> derived order), or -1 on error */
int theme_loader_build_chain(const ThemeLoader *tl,
                             const char *slug,
                             char chain[][MAX_THEME_SLUG], int maxchain) {
    char visited[MAX_THEMES][MAX_THEME_SLUG];
    int  nvisited = 0;
    char tmp_chain[MAX_THEMES][MAX_THEME_SLUG];
    int  chain_len = 0;

    const char *current = slug;
    for (;;) {
        /* Cycle check */
        for (int i = 0; i < nvisited; i++) {
            if (strcmp(visited[i], current) == 0) {
                fprintf(stderr, "Inheritance cycle involving theme %s\n", current);
                return -1;
            }
        }
        snprintf(visited[nvisited++], MAX_THEME_SLUG, "%s", current);

        if (chain_len >= maxchain) return -1;
        snprintf(tmp_chain[chain_len++], MAX_THEME_SLUG, "%s", current);

        if (strcmp(current, "_base") == 0) break;

        const ThemeManifest *m = theme_loader_get_manifest(tl, current);
        if (!m) {
            fprintf(stderr, "Theme %s not found\n", current);
            return -1;
        }

        const char *parent = m->inherits;
        if (!parent[0]) {
            /* Append _base */
            if (chain_len >= maxchain) return -1;
            snprintf(tmp_chain[chain_len++], MAX_THEME_SLUG, "_base");
            break;
        }

        if (strcmp(parent, "_base") != 0) {
            if (!theme_loader_get_manifest(tl, parent)) {
                fprintf(stderr, "Theme %s inherits unknown %s\n", current, parent);
                return -1;
            }
        }
        current = parent;
    }

    /* Reverse: base first */
    for (int i = 0; i < chain_len; i++)
        snprintf(chain[i], MAX_THEME_SLUG, "%s", tmp_chain[chain_len - 1 - i]);

    return chain_len;
}

/* ------------------------------------------------------------------ */
/* File resolution                                                      */
/* ------------------------------------------------------------------ */

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int add_file(char files[][512], int *count, int max,
                    char seen[][512], int *nseen, const char *path) {
    /* Dedup */
    for (int i = 0; i < *nseen; i++) {
        if (strcmp(seen[i], path) == 0) return 0;
    }
    if (!file_exists(path)) return 0;
    if (*count >= max || *nseen >= max) return -1;
    snprintf(seen[*nseen], 512, "%s", path);
    (*nseen)++;
    snprintf(files[*count], 512, "%s", path);
    (*count)++;
    return 0;
}

/* Simple glob: find all *.html in a directory */
static int glob_html(const char *dir,
                     char files[][512], int *count, int max,
                     char seen[][512], int *nseen) {
    DIR *d = opendir(dir);
    if (!d) return 0;

    /* Collect names first for sorting */
    char names[256][256];
    int  nnames = 0;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        size_t nl = strlen(de->d_name);
        if (nl > 5 && strcmp(de->d_name + nl - 5, ".html") == 0) {
            if (nnames < 256) {
                snprintf(names[nnames++], 256, "%s", de->d_name);
            }
        }
    }
    closedir(d);

    /* Sort */
    qsort(names, (size_t)nnames, 256, slug_cmp);

    for (int i = 0; i < nnames; i++) {
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir, names[i]);
        add_file(files, count, max, seen, nseen, full);
    }
    return 0;
}

/* Recursively glob *.html in dir and subdirs */
static void glob_html_recursive(const char *dir,
                                 char files[][512], int *count, int max,
                                 char seen[][512], int *nseen) {
    DIR *d = opendir(dir);
    if (!d) return;

    char subdirs[64][256];
    int  nsub = 0;
    char htmlnames[256][256];
    int  nhtmlnames = 0;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (nsub < 64) snprintf(subdirs[nsub++], 256, "%s", de->d_name);
        } else {
            size_t nl = strlen(de->d_name);
            if (nl > 5 && strcmp(de->d_name + nl - 5, ".html") == 0) {
                if (nhtmlnames < 256) snprintf(htmlnames[nhtmlnames++], 256, "%s", de->d_name);
            }
        }
    }
    closedir(d);

    qsort(htmlnames, (size_t)nhtmlnames, 256, slug_cmp);
    for (int i = 0; i < nhtmlnames; i++) {
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir, htmlnames[i]);
        add_file(files, count, max, seen, nseen, full);
    }

    qsort(subdirs, (size_t)nsub, 256, slug_cmp);
    for (int i = 0; i < nsub; i++) {
        char sub[512];
        snprintf(sub, sizeof(sub), "%s/%s", dir, subdirs[i]);
        glob_html_recursive(sub, files, count, max, seen, nseen);
    }
}

int theme_loader_resolve_files(const ThemeLoader *tl,
                               const char *theme_slug,
                               const char *step,
                               char files[][512], int max_files) {
    char chain[MAX_THEMES][MAX_THEME_SLUG];
    int chain_len = theme_loader_build_chain(tl, theme_slug, chain, MAX_THEMES);
    if (chain_len < 0) return -1;

    char seen[MAX_FILES][512];
    int nseen = 0;
    int count = 0;

    for (int ci = 0; ci < chain_len; ci++) {
        char dir[512];
        snprintf(dir, sizeof(dir), "%s/%s", tl->themes_root, chain[ci]);

        char path[512];
        /* layouts/base.html */
        snprintf(path, sizeof(path), "%s/layouts/base.html", dir);
        add_file(files, &count, max_files, seen, &nseen, path);

        /* layouts/checkout.html */
        snprintf(path, sizeof(path), "%s/layouts/checkout.html", dir);
        add_file(files, &count, max_files, seen, &nseen, path);

        /* partials - recursive html scan */
        char partials_dir[512];
        snprintf(partials_dir, sizeof(partials_dir), "%s/partials", dir);
        glob_html_recursive(partials_dir, files, &count, max_files, seen, &nseen);

        /* steps/<step>.html */
        if (step && step[0]) {
            snprintf(path, sizeof(path), "%s/steps/%s.html", dir, step);
            add_file(files, &count, max_files, seen, &nseen, path);
        }
    }

    return count;
}
