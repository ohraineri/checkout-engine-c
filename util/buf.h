#ifndef UTIL_BUF_H
#define UTIL_BUF_H

#include <stddef.h>
#include <stdarg.h>

/* Growing string buffer */
typedef struct {
    char   *data;
    size_t  len;
    size_t  cap;
} Buf;

void buf_init(Buf *b);
void buf_free(Buf *b);
void buf_reset(Buf *b);

/* Append n bytes */
int buf_append(Buf *b, const char *data, size_t n);
/* Append NUL-terminated string */
int buf_appendz(Buf *b, const char *s);
/* Append single character */
int buf_appendc(Buf *b, char c);
/* Append formatted string (printf-style) */
int buf_printf(Buf *b, const char *fmt, ...);
int buf_vprintf(Buf *b, const char *fmt, va_list ap);

#endif /* UTIL_BUF_H */
