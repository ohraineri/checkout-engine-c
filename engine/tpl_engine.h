#ifndef ENGINE_TPL_ENGINE_H
#define ENGINE_TPL_ENGINE_H

#include "engine/value.h"
#include "util/buf.h"

/* ------------------------------------------------------------------ */
/* Node types                                                            */
/* ------------------------------------------------------------------ */

typedef enum {
    NODE_TEXT,
    NODE_PRINT,      /* {{ .Path }} or {{ .Method arg }} */
    NODE_IF,
    NODE_RANGE,
    NODE_TEMPLATE,
    NODE_DEFINE,
    NODE_BLOCK,
} NodeType;

typedef struct Node Node;

struct Node {
    NodeType type;
    Node    *next;   /* sibling in a list */

    /* NODE_TEXT */
    char    *text;

    /* NODE_PRINT */
    char    *path;       /* e.g. "Cart.Total.Display" */
    char    *method_arg; /* e.g. "core.css" for Assets.URL */

    /* NODE_IF */
    char    *cond_path;
    Node    *if_body;
    Node    *else_body;

    /* NODE_RANGE */
    char    *range_path;
    Node    *range_body;

    /* NODE_TEMPLATE / NODE_DEFINE / NODE_BLOCK */
    char    *tpl_name;
    Node    *block_default; /* for NODE_BLOCK */
};

/* ------------------------------------------------------------------ */
/* TplSet: collection of named templates                                */
/* ------------------------------------------------------------------ */

#define TPLSET_MAX 256

typedef struct {
    char  *name;
    Node  *body;
} TplDef;

typedef struct {
    TplDef defs[TPLSET_MAX];
    int    count;
} TplSet;

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/* Parse multiple HTML files and collect all {{define}} blocks.
 * Returns allocated TplSet* or NULL on error. */
TplSet *tpl_parse_files(const char *paths[], int n);

/* Free a TplSet (and all nodes) */
void tpl_set_free(TplSet *ts);

/* Render the named template into buf, with dot=root context.
 * i18n_messages: optional StrMap* for the `t` helper.
 * assets_base_url / assets_files: for Assets.URL.
 * Returns 0 on success. */
int tpl_render(TplSet *ts, const char *name, Value *dot,
               Buf *out);

#endif /* ENGINE_TPL_ENGINE_H */
