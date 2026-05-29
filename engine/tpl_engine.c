#include "engine/tpl_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* Internal node constructors                                           */
/* ------------------------------------------------------------------ */

static Node *node_new(NodeType type) {
    Node *n = calloc(1, sizeof(Node));
    if (n) n->type = type;
    return n;
}

static void node_free(Node *n) {
    while (n) {
        Node *next = n->next;
        free(n->text);
        free(n->path);
        free(n->method_arg);
        free(n->cond_path);
        free(n->range_path);
        free(n->tpl_name);
        node_free(n->if_body);
        node_free(n->else_body);
        node_free(n->range_body);
        node_free(n->block_default);
        free(n);
        n = next;
    }
}

/* ------------------------------------------------------------------ */
/* TplSet helpers                                                        */
/* ------------------------------------------------------------------ */

static TplDef *tplset_find(TplSet *ts, const char *name) {
    for (int i = 0; i < ts->count; i++) {
        if (strcmp(ts->defs[i].name, name) == 0)
            return &ts->defs[i];
    }
    return NULL;
}

static int tplset_define(TplSet *ts, const char *name, Node *body) {
    /* Overwrite if exists (later file wins) */
    TplDef *existing = tplset_find(ts, name);
    if (existing) {
        node_free(existing->body);
        existing->body = body;
        return 0;
    }
    if (ts->count >= TPLSET_MAX) return -1;
    ts->defs[ts->count].name = strdup(name);
    ts->defs[ts->count].body = body;
    ts->count++;
    return 0;
}

void tpl_set_free(TplSet *ts) {
    if (!ts) return;
    for (int i = 0; i < ts->count; i++) {
        free(ts->defs[i].name);
        node_free(ts->defs[i].body);
    }
    free(ts);
}

/* ------------------------------------------------------------------ */
/* Tokenizer / parser                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *src;
    size_t      pos;
    size_t      len;
} Parser;

static void parser_init(Parser *p, const char *src) {
    p->src = src;
    p->pos = 0;
    p->len = strlen(src);
}

static int parser_eof(const Parser *p) {
    return p->pos >= p->len;
}

/* Find next {{ in src starting at pos.
 * Returns offset from pos, or -1 if not found. */
static ssize_t find_action(const Parser *p) {
    const char *start = p->src + p->pos;
    const char *found = strstr(start, "{{");
    if (!found) return -1;
    return (ssize_t)(found - start);
}

/* Skip whitespace */
static void skip_ws(const char **s) {
    while (**s == ' ' || **s == '\t' || **s == '\n' || **s == '\r') (*s)++;
}

/* Read until end of {{ ... }} block. pos should be right after "{{".
 * Fills token with the trimmed inner content. */
static int read_action(Parser *p, char *token, int toksz) {
    /* Skip leading whitespace and optional '-' */
    while (p->pos < p->len &&
           (p->src[p->pos] == ' ' || p->src[p->pos] == '\t' ||
            p->src[p->pos] == '-'))
        p->pos++;

    size_t start = p->pos;
    /* Find }} */
    while (p->pos + 1 < p->len) {
        if (p->src[p->pos] == '}' && p->src[p->pos+1] == '}')
            break;
        p->pos++;
    }
    size_t end = p->pos;
    if (p->pos + 1 < p->len) p->pos += 2; /* skip }} */

    /* Trim trailing whitespace and optional '-' */
    while (end > start && (p->src[end-1] == ' ' || p->src[end-1] == '\t' ||
                            p->src[end-1] == '-'))
        end--;

    int n = (int)(end - start);
    if (n < 0) n = 0;
    if (n >= toksz) n = toksz - 1;
    memcpy(token, p->src + start, (size_t)n);
    token[n] = '\0';
    return 0;
}

/* ------------------------------------------------------------------ */
/* Unquote a string argument like "core.css"                            */
/* ------------------------------------------------------------------ */

static void unquote_arg(char *s) {
    size_t len = strlen(s);
    if (len >= 2 &&
        ((s[0] == '"' && s[len-1] == '"') ||
         (s[0] == '\'' && s[len-1] == '\''))) {
        memmove(s, s+1, len-2);
        s[len-2] = '\0';
    }
}

/* ------------------------------------------------------------------ */
/* Parse a dot path expression: ".Cart.Total.Display" -> "Cart.Total.Display" */
/* Returns a newly allocated string. */
/* ------------------------------------------------------------------ */

static char *parse_path_expr(const char *expr) {
    /* expr is like ".Cart.Total.Display" or just "." for current dot */
    const char *p = expr;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '.') {
        p++;
        if (*p == '\0' || *p == ' ') return strdup(""); /* bare dot */
    }
    /* Strip trailing spaces */
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t')) len--;
    char *result = malloc(len+1);
    if (!result) return NULL;
    memcpy(result, p, len);
    result[len] = '\0';
    return result;
}

/* ------------------------------------------------------------------ */
/* Forward declaration for recursive parser                             */
/* ------------------------------------------------------------------ */

static Node *parse_node_list(Parser *p, TplSet *ts,
                              const char *stop1, const char *stop2,
                              char *found_stop, int stop_sz);

/* Parse a single action token into a Node.
 * Returns NULL if this is a control keyword (end, else, etc).
 * found_kw receives the keyword if it's a terminator. */
static Node *parse_action_token(Parser *p, TplSet *ts,
                                 const char *token,
                                 char *found_kw, int kw_sz) {
    const char *t = token;
    while (*t == ' ' || *t == '\t') t++;

    /* Detect keywords */
    if (strncmp(t, "define ", 7) == 0 || strncmp(t, "define\"", 7) == 0) {
        /* {{ define "name" }} */
        const char *q = t + 7;
        while (*q == ' ' || *q == '"' || *q == '\'') { if (*q != ' ') break; q++; }
        /* Find name between quotes */
        char name[128] = "";
        if (*q == '"' || *q == '\'') {
            char qc = *q++;
            int ni = 0;
            while (*q && *q != qc && ni < 127) name[ni++] = *q++;
            name[ni] = '\0';
        }
        /* Parse body until {{ end }} */
        char stop[32];
        Node *body = parse_node_list(p, ts, "end", NULL, stop, sizeof(stop));
        tplset_define(ts, name, body);
        return NULL; /* define produces no output node itself */
    }

    if (strncmp(t, "block ", 6) == 0) {
        /* {{ block "name" . }} */
        const char *q = t + 6;
        while (*q == ' ') q++;
        char name[128] = "";
        if (*q == '"' || *q == '\'') {
            char qc = *q++;
            int ni = 0;
            while (*q && *q != qc && ni < 127) name[ni++] = *q++;
            name[ni] = '\0';
        }
        /* Parse default body until {{ end }} */
        char stop[32];
        Node *defbody = parse_node_list(p, ts, "end", NULL, stop, sizeof(stop));

        /* Block: defines itself with default, then emits a template include */
        /* Only define if not already defined (first definition wins for blocks) */
        if (!tplset_find(ts, name)) {
            /* clone isn't trivial; just register it */
            tplset_define(ts, name, defbody);
        } else {
            node_free(defbody);
        }

        Node *n = node_new(NODE_TEMPLATE);
        n->tpl_name = strdup(name);
        return n;
    }

    if (strncmp(t, "template ", 9) == 0) {
        /* {{ template "name" . }} */
        const char *q = t + 9;
        while (*q == ' ') q++;
        char name[128] = "";
        if (*q == '"' || *q == '\'') {
            char qc = *q++;
            int ni = 0;
            while (*q && *q != qc && ni < 127) name[ni++] = *q++;
            name[ni] = '\0';
        }
        Node *n = node_new(NODE_TEMPLATE);
        n->tpl_name = strdup(name);
        return n;
    }

    if (strncmp(t, "if ", 3) == 0) {
        /* {{ if .Path }} */
        const char *q = t + 3;
        while (*q == ' ') q++;
        Node *n = node_new(NODE_IF);
        n->cond_path = parse_path_expr(q);
        char stop[32];
        n->if_body = parse_node_list(p, ts, "end", "else", stop, sizeof(stop));
        if (strcmp(stop, "else") == 0) {
            n->else_body = parse_node_list(p, ts, "end", NULL, stop, sizeof(stop));
        }
        return n;
    }

    if (strncmp(t, "range ", 6) == 0) {
        /* {{ range .Cart.Items }} */
        const char *q = t + 6;
        while (*q == ' ') q++;
        Node *n = node_new(NODE_RANGE);
        n->range_path = parse_path_expr(q);
        char stop[32];
        n->range_body = parse_node_list(p, ts, "end", NULL, stop, sizeof(stop));
        return n;
    }

    /* Terminator keywords */
    if (strcmp(t, "end") == 0 || strcmp(t, "else") == 0) {
        snprintf(found_kw, kw_sz, "%s", t);
        return NULL;
    }

    /* Print expression: .Path, .Path.Method "arg", .Path.Method */
    if (t[0] == '.') {
        /* Check if it's a method call with argument, e.g.:
         * .Assets.URL "core.css"
         * .I18n.MessagesJSON
         * .Cart.Total.Display  */
        Node *n = node_new(NODE_PRINT);

        /* Find space to split path from argument */
        const char *sp = strchr(t, ' ');
        if (sp) {
            /* path up to space */
            int plen = (int)(sp - t);
            char pathbuf[256];
            if (plen < 256) {
                memcpy(pathbuf, t, (size_t)plen);
                pathbuf[plen] = '\0';
            } else {
                snprintf(pathbuf, sizeof(pathbuf), "%s", t);
            }
            n->path = parse_path_expr(pathbuf);
            /* argument: trim quotes */
            const char *arg = sp + 1;
            while (*arg == ' ') arg++;
            char argbuf[256];
            snprintf(argbuf, sizeof(argbuf), "%s", arg);
            unquote_arg(argbuf);
            n->method_arg = strdup(argbuf);
        } else {
            n->path = parse_path_expr(t);
        }
        return n;
    }

    /* Unknown / unsupported - emit as empty */
    return NULL;
}

/* ------------------------------------------------------------------ */
/* parse_node_list: parse nodes until stop keyword                      */
/* ------------------------------------------------------------------ */

static Node *parse_node_list(Parser *p, TplSet *ts,
                              const char *stop1, const char *stop2,
                              char *found_stop, int stop_sz) {
    Node  head;
    Node *tail = &head;
    head.next  = NULL;
    if (found_stop) found_stop[0] = '\0';

    while (!parser_eof(p)) {
        ssize_t offset = find_action(p);

        if (offset < 0) {
            /* No more actions: rest is text */
            size_t remaining = p->len - p->pos;
            if (remaining > 0) {
                Node *n = node_new(NODE_TEXT);
                n->text = malloc(remaining + 1);
                if (n->text) {
                    memcpy(n->text, p->src + p->pos, remaining);
                    n->text[remaining] = '\0';
                }
                tail->next = n;
                tail = n;
            }
            p->pos = p->len;
            break;
        }

        if (offset > 0) {
            /* Text before {{ */
            Node *n = node_new(NODE_TEXT);
            n->text = malloc((size_t)offset + 1);
            if (n->text) {
                memcpy(n->text, p->src + p->pos, (size_t)offset);
                n->text[offset] = '\0';
            }
            tail->next = n;
            tail = n;
            p->pos += (size_t)offset;
        }

        /* Skip {{ */
        p->pos += 2;

        char token[1024];
        read_action(p, token, sizeof(token));

        char kw[32] = "";
        Node *n = parse_action_token(p, ts, token, kw, sizeof(kw));

        if (kw[0]) {
            /* Check if it's a stop */
            if ((stop1 && strcmp(kw, stop1) == 0) ||
                (stop2 && strcmp(kw, stop2) == 0)) {
                if (found_stop) snprintf(found_stop, stop_sz, "%s", kw);
                break;
            }
        }

        if (n) {
            tail->next = n;
            tail = n;
        }
    }

    return head.next;
}

/* ------------------------------------------------------------------ */
/* Parse all files                                                       */
/* ------------------------------------------------------------------ */

TplSet *tpl_parse_files(const char *paths[], int n) {
    TplSet *ts = calloc(1, sizeof(TplSet));
    if (!ts) return NULL;

    for (int i = 0; i < n; i++) {
        FILE *f = fopen(paths[i], "r");
        if (!f) {
            fprintf(stderr, "Cannot open template file: %s\n", paths[i]);
            continue;
        }
        fseek(f, 0, SEEK_END);
        long fsz = ftell(f);
        rewind(f);
        if (fsz <= 0) { fclose(f); continue; }

        char *src = malloc((size_t)fsz + 1);
        if (!src) { fclose(f); tpl_set_free(ts); return NULL; }
        fread(src, 1, (size_t)fsz, f);
        src[fsz] = '\0';
        fclose(f);

        Parser p;
        parser_init(&p, src);

        /* For files that are top-level (not wrapped in define), we need to
         * parse the whole file and look for define blocks.
         * parse_node_list will call parse_action_token which calls tplset_define
         * for {{define}} blocks. Top-level text/nodes outside define are discarded. */
        char stop[32];
        Node *top = parse_node_list(&p, ts, NULL, NULL, stop, sizeof(stop));
        /* Discard top-level nodes that are outside any define
         * (base.html has its HTML directly - it IS the "base.html" template) */

        /* If the file has a "base name" (e.g. base.html), register top-level content */
        /* as that name too, so {{ template "base.html" . }} works */
        const char *basename = paths[i];
        /* Find last slash */
        const char *sl = strrchr(basename, '/');
        if (sl) basename = sl + 1;

        if (!tplset_find(ts, basename) && top) {
            tplset_define(ts, basename, top);
        } else {
            node_free(top);
        }

        free(src);
    }

    return ts;
}

/* ------------------------------------------------------------------ */
/* Renderer                                                              */
/* ------------------------------------------------------------------ */

/* Resolve a dot path against the given dot value.
 * If path is empty (""), returns dot itself. */
static Value *resolve_path(Value *dot, Value *root, const char *path) {
    if (!path || path[0] == '\0') return dot;
    /* Try dot first */
    Value *v = val_get_path(dot, path);
    if (v) return v;
    /* Try root (for accessing top-level fields inside range) */
    if (root && root != dot) {
        v = val_get_path(root, path);
    }
    return v;
}

/* Forward declaration */
static int render_nodes(TplSet *ts, Node *nodes,
                        Value *dot, Value *root, Buf *out);

static void html_escape(Buf *out, const char *s) {
    for (; *s; s++) {
        switch (*s) {
        case '&':  buf_appendz(out, "&amp;"); break;
        case '<':  buf_appendz(out, "&lt;"); break;
        case '>':  buf_appendz(out, "&gt;"); break;
        case '"':  buf_appendz(out, "&#34;"); break;
        case '\'': buf_appendz(out, "&#39;"); break;
        default:   buf_appendc(out, *s); break;
        }
    }
}

static int render_node(TplSet *ts, Node *n,
                        Value *dot, Value *root, Buf *out) {
    switch (n->type) {
    case NODE_TEXT:
        buf_appendz(out, n->text);
        break;

    case NODE_PRINT: {
        /* Handle special method calls */
        const char *path = n->path;

        /* .Assets.URL "filename" */
        if (strcmp(path, "Assets.URL") == 0 && n->method_arg) {
            Value *assets = val_get_path(root, "Assets");
            if (assets) {
                Value *files = val_obj_get(assets, "Files");
                Value *base  = val_obj_get(assets, "BaseURL");
                const char *base_url = (base && base->type == V_STRING) ? base->u.s : "";
                const char *file_path = n->method_arg;
                if (files && files->type == V_OBJECT) {
                    Value *mapped = val_obj_get(files, n->method_arg);
                    if (mapped && mapped->type == V_STRING)
                        file_path = mapped->u.s;
                }
                buf_appendz(out, base_url);
                buf_appendz(out, file_path);
            }
            break;
        }

        /* .I18n.MessagesJSON */
        if (strcmp(path, "I18n.MessagesJSON") == 0) {
            Value *i18n = val_get_path(root, "I18n");
            if (i18n) {
                Value *mj = val_obj_get(i18n, "MessagesJSON");
                if (mj && mj->type == V_STRING) {
                    /* Output raw (not escaped) - it's already safe JSON */
                    buf_appendz(out, mj->u.s);
                }
            }
            break;
        }

        /* Regular path */
        Value *v = resolve_path(dot, root, path);
        if (v) {
            char *s = val_to_string(v);
            if (s) {
                html_escape(out, s);
                free(s);
            }
        }
        break;
    }

    case NODE_IF: {
        Value *cond = resolve_path(dot, root, n->cond_path);
        if (val_is_truthy(cond)) {
            render_nodes(ts, n->if_body, dot, root, out);
        } else if (n->else_body) {
            render_nodes(ts, n->else_body, dot, root, out);
        }
        break;
    }

    case NODE_RANGE: {
        Value *arr = resolve_path(dot, root, n->range_path);
        if (arr && arr->type == V_ARRAY) {
            for (size_t i = 0; i < arr->u.arr.count; i++) {
                Value *item = arr->u.arr.items[i];
                /* Inside range, dot = item, root stays the same */
                render_nodes(ts, n->range_body, item, root, out);
            }
        }
        break;
    }

    case NODE_TEMPLATE: {
        TplDef *def = tplset_find(ts, n->tpl_name);
        if (!def) {
            /* Template not found - silently skip */
            break;
        }
        render_nodes(ts, def->body, dot, root, out);
        break;
    }

    case NODE_DEFINE:
    case NODE_BLOCK:
        /* Should not appear in node list at render time */
        break;
    }
    return 0;
}

static int render_nodes(TplSet *ts, Node *nodes,
                        Value *dot, Value *root, Buf *out) {
    for (Node *n = nodes; n; n = n->next) {
        render_node(ts, n, dot, root, out);
    }
    return 0;
}

int tpl_render(TplSet *ts, const char *name, Value *dot, Buf *out) {
    TplDef *def = tplset_find(ts, name);
    if (!def) {
        fprintf(stderr, "Template %s not found\n", name);
        return -1;
    }
    return render_nodes(ts, def->body, dot, dot, out);
}
