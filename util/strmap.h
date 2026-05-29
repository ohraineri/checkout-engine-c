#ifndef UTIL_STRMAP_H
#define UTIL_STRMAP_H

#include <stddef.h>

/* String -> string hashmap (open addressing, owns copies of keys/values) */

typedef struct StrMapEntry {
    char *key;
    char *value;
    struct StrMapEntry *next;
} StrMapEntry;

typedef struct {
    StrMapEntry **buckets;
    size_t        nbuckets;
    size_t        count;
} StrMap;

StrMap *strmap_new(void);
void    strmap_free(StrMap *m);

/* Returns 0 on success, -1 on OOM */
int     strmap_set(StrMap *m, const char *key, const char *value);

/* Returns value or NULL if not found */
const char *strmap_get(const StrMap *m, const char *key);

/* Iteration: call strmap_iter repeatedly; returns 1 while entries remain.
 * Initialize *idx = 0, *entry = NULL before first call. */
int     strmap_iter(const StrMap *m, size_t *idx, StrMapEntry **entry);

size_t  strmap_count(const StrMap *m);

#endif /* UTIL_STRMAP_H */
