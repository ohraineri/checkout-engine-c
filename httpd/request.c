#define _GNU_SOURCE
#include "httpd/request.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* URL decode                                                            */
/* ------------------------------------------------------------------ */

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void url_decode(const char *src, char *dst, int dstsz) {
    int di = 0;
    for (const char *s = src; *s && di < dstsz - 1; s++) {
        if (*s == '%' && s[1] && s[2]) {
            int h = hex_val(s[1]);
            int l = hex_val(s[2]);
            if (h >= 0 && l >= 0) {
                dst[di++] = (char)((h << 4) | l);
                s += 2;
                continue;
            }
        } else if (*s == '+') {
            dst[di++] = ' ';
            continue;
        }
        dst[di++] = *s;
    }
    dst[di] = '\0';
}

/* ------------------------------------------------------------------ */
/* Parse query string                                                    */
/* ------------------------------------------------------------------ */

static void parse_query(const char *query, Request *req) {
    char buf[REQ_MAX_QUERY];
    snprintf(buf, sizeof(buf), "%s", query);

    char *tok = buf;
    char *amp;
    while (tok && *tok && req->qparam_count < REQ_MAX_PARAMS) {
        amp = strchr(tok, '&');
        if (amp) *amp = '\0';

        char *eq = strchr(tok, '=');
        QueryParam *qp = &req->qparams[req->qparam_count];
        if (eq) {
            int klen = (int)(eq - tok);
            if (klen >= 128) klen = 127;
            memcpy(qp->key, tok, (size_t)klen);
            qp->key[klen] = '\0';
            url_decode(eq + 1, qp->value, sizeof(qp->value));
        } else {
            snprintf(qp->key, sizeof(qp->key), "%s", tok);
            qp->value[0] = '\0';
        }

        if (qp->key[0]) req->qparam_count++;

        tok = amp ? amp + 1 : NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Read a line from fd (up to maxlen bytes, strips \r\n)                */
/* Returns bytes read or -1 on error/EOF */
/* ------------------------------------------------------------------ */

static int read_line(int fd, char *buf, int maxlen) {
    int n = 0;
    char c;
    while (n < maxlen - 1) {
        int r = (int)read(fd, &c, 1);
        if (r <= 0) return -1;
        if (c == '\n') break;
        if (c != '\r') buf[n++] = c;
    }
    buf[n] = '\0';
    return n;
}

/* ------------------------------------------------------------------ */
/* request_parse                                                         */
/* ------------------------------------------------------------------ */

int request_parse(int fd, Request *req) {
    memset(req, 0, sizeof(*req));

    /* Read request line */
    char line[4096];
    if (read_line(fd, line, sizeof(line)) < 0) return -1;

    /* Parse: METHOD /path?query HTTP/1.1 */
    char *sp1 = strchr(line, ' ');
    if (!sp1) return -1;
    *sp1 = '\0';
    snprintf(req->method, sizeof(req->method), "%s", line);

    char *sp2 = strchr(sp1 + 1, ' ');
    if (sp2) *sp2 = '\0';

    char *rawpath = sp1 + 1;
    char *qmark = strchr(rawpath, '?');
    if (qmark) {
        *qmark = '\0';
        snprintf(req->query, sizeof(req->query), "%s", qmark + 1);
        parse_query(req->query, req);
    }
    url_decode(rawpath, req->path, sizeof(req->path));

    /* Read headers */
    while (1) {
        if (read_line(fd, line, sizeof(line)) < 0) break;
        if (line[0] == '\0') break; /* empty line = end of headers */
        if (req->header_count >= REQ_MAX_HEADERS) continue;

        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';

        Header *h = &req->headers[req->header_count];
        snprintf(h->key, sizeof(h->key), "%s", line);
        char *v = colon + 1;
        while (*v == ' ' || *v == '\t') v++;
        /* strip trailing \r */
        size_t vlen = strlen(v);
        while (vlen > 0 && (v[vlen-1] == '\r' || v[vlen-1] == ' ')) v[--vlen] = '\0';
        snprintf(h->value, sizeof(h->value), "%s", v);
        req->header_count++;
    }

    /* Extract session cookie */
    const char *cookie = request_header(req, "Cookie");
    if (cookie) {
        const char *tok = strstr(cookie, "checkout_session=");
        if (tok) {
            tok += 17;
            int ci = 0;
            while (*tok && *tok != ';' && ci < 255)
                req->session_cookie[ci++] = *tok++;
            req->session_cookie[ci] = '\0';
        }
    }

    return 0;
}

const char *request_header(const Request *req, const char *name) {
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].key, name) == 0)
            return req->headers[i].value;
    }
    return NULL;
}

const char *request_query(const Request *req, const char *name) {
    for (int i = 0; i < req->qparam_count; i++) {
        if (strcmp(req->qparams[i].key, name) == 0)
            return req->qparams[i].value;
    }
    return NULL;
}

const char *request_path_param(const Request *req, const char *name) {
    for (int i = 0; i < req->path_param_count; i++) {
        if (strcmp(req->path_params[i][0], name) == 0)
            return req->path_params[i][1];
    }
    return NULL;
}
