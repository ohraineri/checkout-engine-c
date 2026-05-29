#ifndef HTTPD_RESPONSE_H
#define HTTPD_RESPONSE_H

#include <stddef.h>
#include "util/buf.h"

#define RESP_MAX_HEADERS 32

typedef struct {
    char key[128];
    char value[512];
} RespHeader;

typedef struct {
    int        status;
    RespHeader headers[RESP_MAX_HEADERS];
    int        header_count;
    Buf        body;
} Response;

void response_init(Response *resp);
void response_free(Response *resp);

void response_set_header(Response *resp, const char *key, const char *value);
void response_set_status(Response *resp, int status);
void response_set_body(Response *resp, const char *data, size_t len);
void response_set_bodystr(Response *resp, const char *s);

/* Send response over fd */
int response_send(Response *resp, int fd);

/* Convenience: send plain text error */
void response_error(Response *resp, int status, const char *msg);

#endif /* HTTPD_RESPONSE_H */
