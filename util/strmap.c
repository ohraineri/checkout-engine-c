#include "util/strmap.h"

#include <stdlib.h>
#include <string.h>

#define STRMAP_INITIAL_BUCKETS 16
#define STRMAP_LOAD_FACTOR     0.75

static unsigned long strmap_hash(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++) != 0)
        h = ((h << 5) + h) + (unsigned long)c;
    return h;
}

StrMap *strmap_new(void) {
    StrMap *m = calloc(1, sizeof(StrMap));
    if (!m) return NULL;
    m->nbuckets = STRMAP_INITIAL_BUCKETS;
    m->buckets  = calloc(m->nbuckets, sizeof(StrMapEntry *));
    if (!m->buckets) { free(m); return NULL; }
    return m;
}

void strmap_free(StrMap *m) {
    if (!m) return;
    for (size_t i = 0; i < m->nbuckets; i++) {
        StrMapEntry *e = m->buckets[i];
        while (e) {
            StrMapEntry *next = e->next;
            free(e->key);
            free(e->value);
            free(e);
            e = next;
        }
    }
    free(m->buckets);
    free(m);
}

static int strmap_resize(StrMap *m) {
    size_t newnb = m->nbuckets * 2;
    StrMapEntry **nb = calloc(newnb, sizeof(StrMapEntry *));
    if (!nb) return -1;
    for (size_t i = 0; i < m->nbuckets; i++) {
        StrMapEntry *e = m->buckets[i];
        while (e) {
            StrMapEntry *next = e->next;
            size_t idx = strmap_hash(e->key) % newnb;
            e->next = nb[idx];
            nb[idx] = e;
            e = next;
        }
    }
    free(m->buckets);
    m->buckets  = nb;
    m->nbuckets = newnb;
    return 0;
}

int strmap_set(StrMap *m, const char *key, const char *value) {
    if ((double)m->count / (double)m->nbuckets > STRMAP_LOAD_FACTOR) {
        if (strmap_resize(m) != 0) return -1;
    }
    size_t idx = strmap_hash(key) % m->nbuckets;
    for (StrMapEntry *e = m->buckets[idx]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            char *v = strdup(value);
            if (!v) return -1;
            free(e->value);
            e->value = v;
            return 0;
        }
    }
    StrMapEntry *e = calloc(1, sizeof(StrMapEntry));
    if (!e) return -1;
    e->key   = strdup(key);
    e->value = strdup(value);
    if (!e->key || !e->value) {
        free(e->key); free(e->value); free(e);
        return -1;
    }
    e->next = m->buckets[idx];
    m->buckets[idx] = e;
    m->count++;
    return 0;
}

const char *strmap_get(const StrMap *m, const char *key) {
    size_t idx = strmap_hash(key) % m->nbuckets;
    for (StrMapEntry *e = m->buckets[idx]; e; e = e->next) {
        if (strcmp(e->key, key) == 0)
            return e->value;
    }
    return NULL;
}

int strmap_iter(const StrMap *m, size_t *idx, StrMapEntry **entry) {
    /* If current entry has a next, advance within chain */
    if (*entry && (*entry)->next) {
        *entry = (*entry)->next;
        return 1;
    }
    /* Move to next bucket */
    size_t start = (*entry) ? *idx + 1 : *idx;
    for (size_t i = start; i < m->nbuckets; i++) {
        if (m->buckets[i]) {
            *idx   = i;
            *entry = m->buckets[i];
            return 1;
        }
    }
    return 0;
}

size_t strmap_count(const StrMap *m) {
    return m ? m->count : 0;
}
