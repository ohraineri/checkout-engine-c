#include "httpd/server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>

/* ------------------------------------------------------------------ */
/* Static file serving                                                   */
/* ------------------------------------------------------------------ */

static const char *guess_mime(const char *path) {
    size_t len = strlen(path);
    if (len > 5 && strcmp(path + len - 5, ".html") == 0) return "text/html; charset=utf-8";
    if (len > 4 && strcmp(path + len - 4, ".css")  == 0) return "text/css; charset=utf-8";
    if (len > 3 && strcmp(path + len - 3, ".js")   == 0) return "application/javascript; charset=utf-8";
    if (len > 4 && strcmp(path + len - 4, ".png")  == 0) return "image/png";
    if (len > 4 && strcmp(path + len - 4, ".jpg")  == 0) return "image/jpeg";
    if (len > 5 && strcmp(path + len - 5, ".jpeg") == 0) return "image/jpeg";
    if (len > 4 && strcmp(path + len - 4, ".svg")  == 0) return "image/svg+xml";
    if (len > 4 && strcmp(path + len - 4, ".ico")  == 0) return "image/x-icon";
    if (len > 5 && strcmp(path + len - 5, ".woff") == 0) return "font/woff";
    if (len > 6 && strcmp(path + len - 6, ".woff2")== 0) return "font/woff2";
    return "application/octet-stream";
}

static void serve_static_file(const char *fs_path, Response *resp) {
    /* Security: no path traversal */
    if (strstr(fs_path, "..")) {
        response_error(resp, 404, "Not Found");
        return;
    }

    FILE *f = fopen(fs_path, "rb");
    if (!f) {
        response_error(resp, 404, "Not Found");
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    rewind(f);

    if (fsz < 0) { fclose(f); response_error(resp, 500, "Error"); return; }

    char *data = malloc((size_t)fsz);
    if (!data) { fclose(f); response_error(resp, 500, "OOM"); return; }
    fread(data, 1, (size_t)fsz, f);
    fclose(f);

    response_set_status(resp, 200);
    response_set_header(resp, "Content-Type", guess_mime(fs_path));
    response_set_body(resp, data, (size_t)fsz);
    free(data);
}

/* ------------------------------------------------------------------ */
/* Route matching                                                        */
/* ------------------------------------------------------------------ */

/* Returns 1 if pattern matches path, extracting path param into req */
static int route_match(const Route *route, Request *req) {
    if (strcmp(req->method, route->method) != 0) return 0;

    if (route->prefix_match) {
        return strncmp(req->path, route->pattern,
                       strlen(route->pattern)) == 0;
    }

    if (!route->has_param) {
        return strcmp(req->path, route->pattern) == 0;
    }

    /* Pattern like "/{storeSlug}/checkout" */
    const char *pp = route->pattern;
    const char *rp = req->path;

    while (*pp && *rp) {
        if (*pp == '{') {
            /* Find end of param */
            const char *pend = strchr(pp, '}');
            if (!pend) return 0;
            pp = pend + 1;

            /* Extract value up to next '/' or end */
            const char *vstart = rp;
            while (*rp && *rp != '/') rp++;
            if (*pp && *pp == '/' && !*rp) return 0; /* expected more path */

            int vlen = (int)(rp - vstart);
            if (vlen <= 0) return 0;

            if (req->path_param_count < REQ_MAX_PARAMS) {
                snprintf(req->path_params[req->path_param_count][0],
                         256, "%s", route->param_name);
                int copylen = vlen < 255 ? vlen : 255;
                memcpy(req->path_params[req->path_param_count][1],
                       vstart, (size_t)copylen);
                req->path_params[req->path_param_count][1][copylen] = '\0';
                req->path_param_count++;
            }
        } else {
            if (*pp != *rp) return 0;
            pp++; rp++;
        }
    }
    return (*pp == '\0' && *rp == '\0');
}

/* ------------------------------------------------------------------ */
/* Per-connection handler                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    int     fd;
    Server *server;
} ConnArgs;

static void *handle_connection(void *arg) {
    ConnArgs *args = (ConnArgs *)arg;
    int fd = args->fd;
    Server *server = args->server;
    free(args);

    Request  req;
    Response resp;
    response_init(&resp);

    if (request_parse(fd, &req) != 0) {
        close(fd);
        return NULL;
    }

    /* Match route */
    int matched = 0;

    /* Check static files prefix */
    const char *static_prefix = "/static/themes/";
    if (strncmp(req.path, static_prefix, strlen(static_prefix)) == 0) {
        /* Strip prefix, serve from themes root */
        const char *rel = req.path + strlen(static_prefix);
        char fs_path[1024];
        snprintf(fs_path, sizeof(fs_path), "%s/%s", server->themes_root, rel);
        serve_static_file(fs_path, &resp);
        matched = 1;
    }

    if (!matched) {
        for (int i = 0; i < server->route_count; i++) {
            if (route_match(&server->routes[i], &req)) {
                server->routes[i].handler(&req, &resp, server->routes[i].userdata);
                matched = 1;
                break;
            }
        }
    }

    if (!matched) {
        response_error(&resp, 404, "Not Found");
    }

    response_send(&resp, fd);
    response_free(&resp);
    close(fd);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Server                                                                */
/* ------------------------------------------------------------------ */

void server_init(Server *s, const char *themes_root) {
    memset(s, 0, sizeof(*s));
    snprintf(s->themes_root, sizeof(s->themes_root), "%s", themes_root);
}

void server_add_route(Server *s, const char *method, const char *pattern,
                      HandlerFunc handler, void *userdata) {
    if (s->route_count >= MAX_ROUTES) return;
    Route *r = &s->routes[s->route_count++];
    snprintf(r->method,  sizeof(r->method),  "%s", method);
    snprintf(r->pattern, sizeof(r->pattern), "%s", pattern);
    r->handler   = handler;
    r->userdata  = userdata;
    r->has_param = 0;

    /* Check for {param} in pattern */
    const char *lb = strchr(pattern, '{');
    if (lb) {
        r->has_param = 1;
        const char *rb = strchr(lb, '}');
        if (rb) {
            int nlen = (int)(rb - lb - 1);
            if (nlen > 0 && nlen < 128) {
                memcpy(r->param_name, lb+1, (size_t)nlen);
                r->param_name[nlen] = '\0';
            }
        }
    }
}

int server_listen_and_serve(Server *s, const char *host, const char *port) {
    /* Ignore SIGPIPE */
    signal(SIGPIPE, SIG_IGN);

    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }

    int lfd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        lfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (lfd < 0) continue;

        int one = 1;
        setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        if (bind(lfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(lfd);
        lfd = -1;
    }
    freeaddrinfo(res);

    if (lfd < 0) {
        perror("bind");
        return -1;
    }

    if (listen(lfd, 128) != 0) {
        perror("listen");
        close(lfd);
        return -1;
    }

    printf("Listening on %s:%s\n", host, port);

    while (1) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        ConnArgs *args = malloc(sizeof(ConnArgs));
        if (!args) { close(cfd); continue; }
        args->fd     = cfd;
        args->server = s;

        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&tid, &attr, handle_connection, args);
        pthread_attr_destroy(&attr);
    }

    close(lfd);
    return 0;
}
