#ifndef HTTPD_REQUEST_H
#define HTTPD_REQUEST_H

#include <stddef.h>

#define REQ_MAX_HEADERS  64
#define REQ_MAX_PATH     2048
#define REQ_MAX_QUERY    2048
#define REQ_MAX_METHOD   16
#define REQ_MAX_HV       1024
#define REQ_MAX_PARAMS   16

typedef struct {
    char key[128];
    char value[REQ_MAX_HV];
} Header;

typedef struct {
    char key[128];
    char value[512];
} QueryParam;

typedef struct {
    char method[REQ_MAX_METHOD];
    char path[REQ_MAX_PATH];
    char query[REQ_MAX_QUERY];

    Header     headers[REQ_MAX_HEADERS];
    int        header_count;

    QueryParam qparams[REQ_MAX_PARAMS];
    int        qparam_count;

    /* Path params set by router (e.g. storeSlug) */
    char path_params[REQ_MAX_PARAMS][2][256];
    int  path_param_count;

    /* Locale/currency set by middleware */
    char locale[64];
    char currency[16];

    /* Cookie: checkout_session */
    char session_cookie[256];
} Request;

/* Parse raw HTTP/1.1 request from fd into req.
 * Returns 0 on success, -1 on error/disconnect. */
int request_parse(int fd, Request *req);

/* Get header value (case-insensitive) or NULL */
const char *request_header(const Request *req, const char *name);

/* Get query param value or NULL */
const char *request_query(const Request *req, const char *name);

/* Get path param value or NULL */
const char *request_path_param(const Request *req, const char *name);

/* URL-decode src into dst (dst must be at least src_len+1 bytes) */
void url_decode(const char *src, char *dst, int dstsz);

#endif /* HTTPD_REQUEST_H */
