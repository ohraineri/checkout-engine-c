#include "handlers/checkout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/context.h"
#include "middleware/middleware.h"

/* ------------------------------------------------------------------ */
/* Demo cart data (matches Go handler)                                   */
/* ------------------------------------------------------------------ */

static void build_cart_data(CartData *cart, const char *currency) {
    memset(cart, 0, sizeof(*cart));

    /* Item 1 */
    CartItem *it1 = &cart->items[0];
    snprintf(it1->id,        sizeof(it1->id),        "item-001");
    snprintf(it1->name,      sizeof(it1->name),      "Camiseta B\xC3\xA1sica");
    snprintf(it1->image_url, sizeof(it1->image_url), "/static/products/camiseta.jpg");
    it1->quantity = 2;
    it1->unit_price.amount = 4990;
    snprintf(it1->unit_price.currency, sizeof(it1->unit_price.currency), "%s", currency);
    it1->total.amount = 9980;
    snprintf(it1->total.currency, sizeof(it1->total.currency), "%s", currency);

    /* Item 2 */
    CartItem *it2 = &cart->items[1];
    snprintf(it2->id,        sizeof(it2->id),        "item-002");
    snprintf(it2->name,      sizeof(it2->name),      "Cal\xC3\xA7""a Slim");
    snprintf(it2->image_url, sizeof(it2->image_url), "/static/products/calca.jpg");
    it2->quantity = 1;
    it2->unit_price.amount = 12990;
    snprintf(it2->unit_price.currency, sizeof(it2->unit_price.currency), "%s", currency);
    it2->total.amount = 12990;
    snprintf(it2->total.currency, sizeof(it2->total.currency), "%s", currency);

    cart->item_count_arr = 2;

    cart->subtotal.amount = 22970;
    snprintf(cart->subtotal.currency, sizeof(cart->subtotal.currency), "%s", currency);
    cart->shipping.amount = 1500;
    snprintf(cart->shipping.currency, sizeof(cart->shipping.currency), "%s", currency);
    cart->discount.amount = 0;
    snprintf(cart->discount.currency, sizeof(cart->discount.currency), "%s", currency);
    cart->total.amount = 24470;
    snprintf(cart->total.currency, sizeof(cart->total.currency), "%s", currency);
    cart->item_count = 3;
    cart->coupon_applied = NULL;
}

static void fill_money_displays(CartData *cart) {
    format_currency(cart->subtotal.amount, cart->subtotal.currency,
                    cart->subtotal.display, sizeof(cart->subtotal.display));
    format_currency(cart->shipping.amount, cart->shipping.currency,
                    cart->shipping.display, sizeof(cart->shipping.display));
    format_currency(cart->discount.amount, cart->discount.currency,
                    cart->discount.display, sizeof(cart->discount.display));
    format_currency(cart->total.amount, cart->total.currency,
                    cart->total.display, sizeof(cart->total.display));
    for (int i = 0; i < cart->item_count_arr; i++) {
        format_currency(cart->items[i].unit_price.amount,
                        cart->items[i].unit_price.currency,
                        cart->items[i].unit_price.display,
                        sizeof(cart->items[i].unit_price.display));
        format_currency(cart->items[i].total.amount,
                        cart->items[i].total.currency,
                        cart->items[i].total.display,
                        sizeof(cart->items[i].total.display));
    }
}

static void build_session(SessionData *sess, const char *session_cookie) {
    memset(sess, 0, sizeof(*sess));
    snprintf(sess->id, sizeof(sess->id), "%s", session_cookie ? session_cookie : "");
    sess->is_guest = 1;
}

static void build_step_state(StepState *step, const char *current,
                              const char *flow_profile) {
    memset(step, 0, sizeof(*step));
    snprintf(step->current, sizeof(step->current), "%s", current);

    if (strcmp(flow_profile, "one-click") == 0) {
        step->total = 1;
        step->step_count = 1;
        snprintf(step->steps[0], sizeof(step->steps[0]), "one-click");
    } else {
        step->total = 3;
        step->step_count = 3;
        snprintf(step->steps[0], sizeof(step->steps[0]), "address");
        snprintf(step->steps[1], sizeof(step->steps[1]), "shipping");
        snprintf(step->steps[2], sizeof(step->steps[2]), "payment");
    }

    /* Calculate progress */
    for (int i = 0; i < step->step_count; i++) {
        if (strcmp(step->steps[i], current) == 0) {
            step->progress = (i + 1) * 100 / step->step_count;
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Render helper                                                         */
/* ------------------------------------------------------------------ */

static void do_render(CheckoutHandler *h,
                      Request *req, Response *resp,
                      const char *theme_slug, const char *step,
                      StoreConfig *store) {
    /* Run locale middleware */
    locale_middleware(req, h->default_locale, h->default_currency);

    /* Run cache middleware */
    cache_middleware(resp, h->cache_max_age,
                     store->slug, theme_slug, step,
                     req->locale, req->currency);

    /* Build cart */
    CartData cart;
    build_cart_data(&cart, store->currency);
    fill_money_displays(&cart);

    /* Build session */
    SessionData sess;
    build_session(&sess, req->session_cookie);

    /* Build step state */
    StepState step_state;
    build_step_state(&step_state, step, store->flow_profile);

    /* Build i18n */
    I18nBundle i18n;
    snprintf(i18n.locale, sizeof(i18n.locale), "%s", store->locale);
    i18n.messages = i18n_loader_load(h->i18n_loader, theme_slug, store->locale);
    /* i18n.messages is owned by cache, don't free */

    /* Features & assets */
    FeatureFlags features;
    features.enabled = NULL;

    AssetManifest assets;
    snprintf(assets.base_url, sizeof(assets.base_url),
             "/static/themes/%s/static/", theme_slug);
    assets.files = NULL;

    /* Build template context */
    TemplateContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.session  = sess;
    ctx.cart     = cart;
    ctx.store    = *store;
    ctx.step     = step_state;
    ctx.i18n     = i18n;
    ctx.features = features;
    ctx.assets   = assets;

    /* Build render input */
    RenderInput in;
    memset(&in, 0, sizeof(in));
    snprintf(in.theme_slug, sizeof(in.theme_slug), "%s", theme_slug);
    snprintf(in.step,       sizeof(in.step),       "%s", step);
    in.ctx = ctx;
    snprintf(in.locale,   sizeof(in.locale),   "%s", req->locale);
    snprintf(in.currency, sizeof(in.currency), "%s", req->currency);

    /* If-None-Match */
    const char *inm = request_header(req, "If-None-Match");
    if (inm) snprintf(in.if_none_match, sizeof(in.if_none_match), "%s", inm);

    RenderResult result;
    if (renderer_render(h->renderer, &in, &result) != 0) {
        response_error(resp, 500, "Internal Server Error");
        return;
    }

    if (result.not_modified) {
        response_set_status(resp, 304);
        response_set_header(resp, "ETag", result.etag);
        render_result_free(&result);
        return;
    }

    response_set_status(resp, 200);
    response_set_header(resp, "Content-Type", "text/html; charset=utf-8");
    response_set_header(resp, "ETag", result.etag);
    response_set_header(resp, "X-Theme", theme_slug);
    response_set_header(resp, "X-Step",  step);

    response_set_body(resp, result.body.data, result.body.len);
    render_result_free(&result);
}

/* ------------------------------------------------------------------ */
/* Route handlers                                                        */
/* ------------------------------------------------------------------ */

static void handle_dev_checkout(Request *req, Response *resp, void *userdata) {
    CheckoutHandler *h = (CheckoutHandler *)userdata;

    StoreConfig store;
    memset(&store, 0, sizeof(store));
    snprintf(store.slug,         sizeof(store.slug),         "dev-store");
    snprintf(store.name,         sizeof(store.name),         "Dev Store");
    snprintf(store.theme_slug,   sizeof(store.theme_slug),   "dev-one-click");
    snprintf(store.flow_profile, sizeof(store.flow_profile), "one-click");
    snprintf(store.locale,       sizeof(store.locale),       "%s",
             h->default_locale);
    snprintf(store.currency,     sizeof(store.currency),     "BRL");
    snprintf(store.tokens.primary_color,   sizeof(store.tokens.primary_color),   "#1a56db");
    snprintf(store.tokens.secondary_color, sizeof(store.tokens.secondary_color), "#ffffff");
    snprintf(store.tokens.font_family,     sizeof(store.tokens.font_family),     "sans-serif");
    snprintf(store.tokens.border_radius,   sizeof(store.tokens.border_radius),   "6px");

    do_render(h, req, resp, "dev-one-click", "one-click", &store);
}

static void handle_store_checkout(Request *req, Response *resp, void *userdata) {
    CheckoutHandler *h = (CheckoutHandler *)userdata;

    /* Locale middleware runs inside do_render */
    locale_middleware(req, h->default_locale, h->default_currency);

    const char *store_slug = request_path_param(req, "storeSlug");
    if (!store_slug || !store_slug[0]) store_slug = "unknown";

    StoreConfig store;
    memset(&store, 0, sizeof(store));
    snprintf(store.slug,         sizeof(store.slug),         "%s", store_slug);
    snprintf(store.name,         sizeof(store.name),         "%s", store_slug);
    snprintf(store.theme_slug,   sizeof(store.theme_slug),   "default");
    snprintf(store.flow_profile, sizeof(store.flow_profile), "multi-step");
    snprintf(store.locale,       sizeof(store.locale),       "%s", req->locale);
    snprintf(store.currency,     sizeof(store.currency),     "BRL");
    snprintf(store.tokens.primary_color,   sizeof(store.tokens.primary_color),   "#1d4ed8");
    snprintf(store.tokens.secondary_color, sizeof(store.tokens.secondary_color), "#ffffff");
    snprintf(store.tokens.font_family,     sizeof(store.tokens.font_family),     "sans-serif");
    snprintf(store.tokens.border_radius,   sizeof(store.tokens.border_radius),   "8px");

    /* Resolve step */
    const char *step = "multi-step";
    if (strcmp(store.flow_profile, "one-click") == 0)
        step = "one-click";

    do_render(h, req, resp, store.theme_slug, step, &store);
}

/* ------------------------------------------------------------------ */
/* Public API                                                            */
/* ------------------------------------------------------------------ */

CheckoutHandler *checkout_handler_new(Renderer *renderer,
                                       ThemeLoader *loader,
                                       I18nLoader *i18n_loader,
                                       const char *default_locale,
                                       const char *default_currency,
                                       int cache_max_age) {
    CheckoutHandler *h = calloc(1, sizeof(CheckoutHandler));
    if (!h) return NULL;
    h->renderer     = renderer;
    h->loader       = loader;
    h->i18n_loader  = i18n_loader;
    h->cache_max_age = cache_max_age;
    snprintf(h->default_locale,   sizeof(h->default_locale),   "%s", default_locale);
    snprintf(h->default_currency, sizeof(h->default_currency), "%s", default_currency);
    return h;
}

void checkout_handler_free(CheckoutHandler *h) {
    free(h);
}

void checkout_handler_register(CheckoutHandler *h, Server *s) {
    server_add_route(s, "GET", "/dev/checkout",
                     handle_dev_checkout, h);
    server_add_route(s, "GET", "/{storeSlug}/checkout",
                     handle_store_checkout, h);
}
