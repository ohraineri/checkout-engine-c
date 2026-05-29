#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include "engine/theme_loader.h"
#include "engine/template_cache.h"
#include "engine/context.h"
#include "util/buf.h"

typedef struct {
    ThemeLoader   *loader;
    TemplateCache *cache;
} Renderer;

typedef struct {
    char             theme_slug[128];
    char             step[64];
    TemplateContext  ctx;
    char             locale[64];
    char             currency[16];
    char             if_none_match[128]; /* from request header */
} RenderInput;

typedef struct {
    int    status;
    char   etag[128];
    int    not_modified;
    Buf    body;
} RenderResult;

Renderer *renderer_new(ThemeLoader *loader, TemplateCache *cache);
void      renderer_free(Renderer *r);

/* Fills result->body, result->etag, result->status, result->not_modified.
 * Caller must call render_result_free() after use. */
int renderer_render(Renderer *r, const RenderInput *in, RenderResult *result);

void render_result_free(RenderResult *rr);

#endif /* ENGINE_RENDERER_H */
