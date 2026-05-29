#include "httpd/response.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static const char *status_text(int status) {
    switch (status) {
    case 200: return "OK";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default:  return "Unknown";
    }
}

void response_init(Response *resp) {
    memset(resp, 0, sizeof(*resp));
    resp->status = 200;
    buf_init(&resp->body);
}

void response_free(Response *resp) {
    if (!resp) return;
    buf_free(&resp->body);
}

void response_set_header(Response *resp, const char *key, const char *value) {
    /* Update existing */
    for (int i = 0; i < resp->header_count; i++) {
        if (strcasecmp(resp->headers[i].key, key) == 0) {
            snprintf(resp->headers[i].value, sizeof(resp->headers[i].value), "%s", value);
            return;
        }
    }
    if (resp->header_count >= RESP_MAX_HEADERS) return;
    snprintf(resp->headers[resp->header_count].key,   128, "%s", key);
    snprintf(resp->headers[resp->header_count].value, 512, "%s", value);
    resp->header_count++;
}

void response_set_status(Response *resp, int status) {
    resp->status = status;
}

void response_set_body(Response *resp, const char *data, size_t len) {
    buf_reset(&resp->body);
    buf_append(&resp->body, data, len);
}

void response_set_bodystr(Response *resp, const char *s) {
    if (s) response_set_body(resp, s, strlen(s));
}

int response_send(Response *resp, int fd) {
    Buf header_buf;
    buf_init(&header_buf);

    /* Status line */
    buf_printf(&header_buf, "HTTP/1.1 %d %s\r\n",
               resp->status, status_text(resp->status));

    /* Content-Length */
    buf_printf(&header_buf, "Content-Length: %zu\r\n", resp->body.len);

    /* Connection: close */
    buf_appendz(&header_buf, "Connection: close\r\n");

    /* Custom headers */
    for (int i = 0; i < resp->header_count; i++) {
        buf_printf(&header_buf, "%s: %s\r\n",
                   resp->headers[i].key, resp->headers[i].value);
    }

    buf_appendz(&header_buf, "\r\n");

    /* Send header */
    ssize_t sent = write(fd, header_buf.data, header_buf.len);
    buf_free(&header_buf);
    if (sent < 0) return -1;

    /* Send body */
    if (resp->body.len > 0) {
        sent = write(fd, resp->body.data, resp->body.len);
        if (sent < 0) return -1;
    }

    return 0;
}

void response_error(Response *resp, int status, const char *msg) {
    response_set_status(resp, status);
    response_set_header(resp, "Content-Type", "text/plain; charset=utf-8");
    response_set_bodystr(resp, msg);
}
