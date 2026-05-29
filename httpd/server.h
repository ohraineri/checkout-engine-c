#ifndef HTTPD_SERVER_H
#define HTTPD_SERVER_H

#include "httpd/request.h"
#include "httpd/response.h"

/* Handler function type */
typedef void (*HandlerFunc)(Request *req, Response *resp, void *userdata);

/* Route */
#define ROUTE_MAX_PATTERN 256

typedef struct {
    char        method[16];
    char        pattern[ROUTE_MAX_PATTERN];
    HandlerFunc handler;
    void       *userdata;
    /* If pattern has a {param}, param_name holds its name */
    char        param_name[128];
    int         has_param;
    /* prefix match (for static file serving) */
    int         prefix_match;
} Route;

#define MAX_ROUTES 64

typedef struct {
    Route   routes[MAX_ROUTES];
    int     route_count;
    char    static_dir[512];    /* directory for /static/themes/ */
    char    themes_root[512];
} Server;

void server_init(Server *s, const char *themes_root);

/* Register a route. Pattern may include {paramName}. */
void server_add_route(Server *s, const char *method, const char *pattern,
                      HandlerFunc handler, void *userdata);

/* Listen and serve. Blocks forever (or until signal). */
int server_listen_and_serve(Server *s, const char *host, const char *port);

#endif /* HTTPD_SERVER_H */
