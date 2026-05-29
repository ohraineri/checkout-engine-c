#include "util/buf.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BUF_INITIAL_CAP 256

void buf_init(Buf *b) {
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
}

void buf_free(Buf *b) {
    free(b->data);
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
}

void buf_reset(Buf *b) {
    b->len = 0;
}

static int buf_grow(Buf *b, size_t needed) {
    if (b->len + needed <= b->cap) return 0;
    size_t newcap = b->cap == 0 ? BUF_INITIAL_CAP : b->cap * 2;
    while (newcap < b->len + needed) newcap *= 2;
    char *p = realloc(b->data, newcap + 1); /* +1 for NUL */
    if (!p) return -1;
    b->data = p;
    b->cap  = newcap;
    return 0;
}

int buf_append(Buf *b, const char *data, size_t n) {
    if (n == 0) return 0;
    if (buf_grow(b, n) != 0) return -1;
    memcpy(b->data + b->len, data, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

int buf_appendz(Buf *b, const char *s) {
    if (!s) return 0;
    return buf_append(b, s, strlen(s));
}

int buf_appendc(Buf *b, char c) {
    return buf_append(b, &c, 1);
}

int buf_printf(Buf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = buf_vprintf(b, fmt, ap);
    va_end(ap);
    return r;
}

int buf_vprintf(Buf *b, const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (needed < 0) return -1;
    if (buf_grow(b, (size_t)needed + 1) != 0) return -1;
    vsnprintf(b->data + b->len, (size_t)needed + 1, fmt, ap);
    b->len += (size_t)needed;
    b->data[b->len] = '\0';
    return 0;
}
