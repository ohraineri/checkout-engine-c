#include "engine/renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>

Renderer *renderer_new(ThemeLoader *loader, TemplateCache *cache) {
    Renderer *r = calloc(1, sizeof(Renderer));
    if (!r) return NULL;
    r->loader = loader;
    r->cache  = cache;
    return r;
}

void renderer_free(Renderer *r) {
    free(r);
}

void render_result_free(RenderResult *rr) {
    if (!rr) return;
    buf_free(&rr->body);
}

int renderer_render(Renderer *r, const RenderInput *in, RenderResult *result) {
    memset(result, 0, sizeof(*result));
    buf_init(&result->body);
    result->status = 200;

    /* Get compiled template set */
    TplSet *ts = template_cache_get(r->cache, in->theme_slug, in->step);
    if (!ts) {
        result->status = 500;
        fprintf(stderr, "renderer: failed to get template for %s/%s\n",
                in->theme_slug, in->step);
        return -1;
    }

    /* Build Value tree from context */
    Value *dot = build_template_value(&in->ctx);
    if (!dot) {
        result->status = 500;
        return -1;
    }

    /* Render into buffer */
    int rc = tpl_render(ts, "base.html", dot, &result->body);
    val_free(dot);

    if (rc != 0) {
        result->status = 500;
        return -1;
    }

    /* Compute SHA-256 ETag */
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)result->body.data,
           result->body.len, hash);

    char hex[SHA256_DIGEST_LENGTH * 2 + 3];
    hex[0] = '"';
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        snprintf(hex + 1 + i*2, 3, "%02x", hash[i]);
    hex[SHA256_DIGEST_LENGTH*2 + 1] = '"';
    hex[SHA256_DIGEST_LENGTH*2 + 2] = '\0';

    snprintf(result->etag, sizeof(result->etag), "%s", hex);

    /* Check If-None-Match */
    if (in->if_none_match[0] &&
        strcmp(in->if_none_match, result->etag) == 0) {
        result->not_modified = 1;
        result->status = 304;
        buf_free(&result->body);
        buf_init(&result->body);
    }

    return 0;
}
