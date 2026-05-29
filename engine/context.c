#include "engine/context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/buf.h"

/* ------------------------------------------------------------------ */
/* Currency formatting (port of Go formatCurrency)                     */
/* ------------------------------------------------------------------ */

static void insert_thousands(int64_t n, const char *sep, char *out, int outsz) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%lld", (long long)n);
    int len  = (int)strlen(tmp);
    int start = len % 3;
    if (start == 0) start = 3;

    Buf b;
    buf_init(&b);
    buf_append(&b, tmp, (size_t)start);
    for (int i = start; i < len; i += 3) {
        buf_appendz(&b, sep);
        buf_append(&b, tmp + i, 3);
    }
    snprintf(out, outsz, "%s", b.data ? b.data : tmp);
    buf_free(&b);
}

void format_currency(int64_t cents, const char *code, char *out, int outsz) {
    int negative = cents < 0;
    if (negative) cents = -cents;

    int64_t whole = cents / 100;
    int64_t frac  = cents % 100;

    const char *prefix, *tsep, *dsep;
    if (strcmp(code, "BRL") == 0) {
        prefix = "R$ "; tsep = "."; dsep = ",";
    } else if (strcmp(code, "USD") == 0) {
        prefix = "$ "; tsep = ","; dsep = ".";
    } else if (strcmp(code, "EUR") == 0) {
        prefix = "\xE2\x82\xAC "; tsep = "."; dsep = ","; /* € */
    } else {
        /* fallback: code + space */
        char fb[32];
        snprintf(fb, sizeof(fb), "%s ", code);
        prefix = fb; tsep = ","; dsep = ".";
        /* Must build inline because prefix points to local fb */
        char thousands[64];
        insert_thousands(whole, tsep, thousands, sizeof(thousands));
        if (negative)
            snprintf(out, outsz, "-%s%s%s%02lld", fb, thousands, dsep, (long long)frac);
        else
            snprintf(out, outsz, "%s%s%s%02lld", fb, thousands, dsep, (long long)frac);
        return;
    }

    char thousands[64];
    insert_thousands(whole, tsep, thousands, sizeof(thousands));
    if (negative)
        snprintf(out, outsz, "-%s%s%s%02lld", prefix, thousands, dsep, (long long)frac);
    else
        snprintf(out, outsz, "%s%s%s%02lld", prefix, thousands, dsep, (long long)frac);
}

/* ------------------------------------------------------------------ */
/* Helpers to build Value trees                                         */
/* ------------------------------------------------------------------ */

static Value *money_to_val(const Money *m) {
    Value *v = val_object();
    if (!v) return NULL;
    val_obj_set(v, "Amount",   val_int(m->amount));
    val_obj_set(v, "Currency", val_string(m->currency));
    val_obj_set(v, "Display",  val_string(m->display));
    return v;
}

static Value *cart_item_to_val(const CartItem *ci) {
    Value *v = val_object();
    if (!v) return NULL;
    val_obj_set(v, "ID",        val_string(ci->id));
    val_obj_set(v, "Name",      val_string(ci->name));
    val_obj_set(v, "ImageURL",  val_string(ci->image_url));
    val_obj_set(v, "Quantity",  val_int(ci->quantity));
    val_obj_set(v, "UnitPrice", money_to_val(&ci->unit_price));
    val_obj_set(v, "Total",     money_to_val(&ci->total));
    return v;
}

static Value *i18n_messages_json(const StrMap *messages) {
    /* Build a JSON string from the messages map */
    if (!messages) return val_string("{}");
    Buf b;
    buf_init(&b);
    buf_appendc(&b, '{');
    int first = 1;
    size_t idx = 0;
    StrMapEntry *entry = NULL;
    while (strmap_iter(messages, &idx, &entry)) {
        if (!first) buf_appendc(&b, ',');
        first = 0;
        /* JSON-encode key */
        buf_appendc(&b, '"');
        for (const char *p = entry->key; *p; p++) {
            if (*p == '"' || *p == '\\') buf_appendc(&b, '\\');
            buf_appendc(&b, *p);
        }
        buf_appendz(&b, "\":\"");
        /* JSON-encode value */
        for (const char *p = entry->value; *p; p++) {
            if (*p == '"' || *p == '\\') buf_appendc(&b, '\\');
            else if (*p == '\n') { buf_appendc(&b, '\\'); buf_appendc(&b, 'n'); continue; }
            else if (*p == '\r') { buf_appendc(&b, '\\'); buf_appendc(&b, 'r'); continue; }
            else if (*p == '\t') { buf_appendc(&b, '\\'); buf_appendc(&b, 't'); continue; }
            buf_appendc(&b, *p);
        }
        buf_appendc(&b, '"');
    }
    buf_appendc(&b, '}');
    Value *v = val_string(b.data ? b.data : "{}");
    buf_free(&b);
    return v;
}

/* ------------------------------------------------------------------ */
/* build_template_value                                                 */
/* ------------------------------------------------------------------ */

Value *build_template_value(const TemplateContext *ctx) {
    Value *root = val_object();
    if (!root) return NULL;

    /* Session */
    Value *session = val_object();
    val_obj_set(session, "ID",           val_string(ctx->session.id));
    val_obj_set(session, "CustomerName", val_string(ctx->session.customer_name));
    val_obj_set(session, "Email",        val_string(ctx->session.email));
    val_obj_set(session, "IsGuest",      val_bool(ctx->session.is_guest));
    val_obj_set(root, "Session", session);

    /* Cart */
    Value *cart = val_object();
    Value *items_arr = val_array();
    for (int i = 0; i < ctx->cart.item_count_arr; i++) {
        val_arr_push(items_arr, cart_item_to_val(&ctx->cart.items[i]));
    }
    val_obj_set(cart, "Items",     items_arr);
    val_obj_set(cart, "Subtotal",  money_to_val(&ctx->cart.subtotal));
    val_obj_set(cart, "Shipping",  money_to_val(&ctx->cart.shipping));
    val_obj_set(cart, "Discount",  money_to_val(&ctx->cart.discount));
    val_obj_set(cart, "Total",     money_to_val(&ctx->cart.total));
    val_obj_set(cart, "ItemCount", val_int(ctx->cart.item_count));
    if (ctx->cart.coupon_applied) {
        val_obj_set(cart, "CouponApplied", val_string(ctx->cart.coupon_applied));
    } else {
        val_obj_set(cart, "CouponApplied", val_null());
    }
    val_obj_set(root, "Cart", cart);

    /* Store */
    Value *store = val_object();
    val_obj_set(store, "Slug",        val_string(ctx->store.slug));
    val_obj_set(store, "Name",        val_string(ctx->store.name));
    val_obj_set(store, "LogoURL",     val_string(ctx->store.logo_url));
    val_obj_set(store, "ThemeSlug",   val_string(ctx->store.theme_slug));
    val_obj_set(store, "FlowProfile", val_string(ctx->store.flow_profile));
    val_obj_set(store, "Locale",      val_string(ctx->store.locale));
    val_obj_set(store, "Currency",    val_string(ctx->store.currency));
    Value *tokens = val_object();
    val_obj_set(tokens, "PrimaryColor",   val_string(ctx->store.tokens.primary_color));
    val_obj_set(tokens, "SecondaryColor", val_string(ctx->store.tokens.secondary_color));
    val_obj_set(tokens, "FontFamily",     val_string(ctx->store.tokens.font_family));
    val_obj_set(tokens, "BorderRadius",   val_string(ctx->store.tokens.border_radius));
    val_obj_set(store, "Tokens", tokens);
    val_obj_set(root, "Store", store);

    /* Step */
    Value *step = val_object();
    val_obj_set(step, "Current",  val_string(ctx->step.current));
    val_obj_set(step, "Total",    val_int(ctx->step.total));
    val_obj_set(step, "Progress", val_int(ctx->step.progress));
    Value *steps_arr = val_array();
    for (int i = 0; i < ctx->step.step_count; i++)
        val_arr_push(steps_arr, val_string(ctx->step.steps[i]));
    val_obj_set(step, "Steps", steps_arr);
    val_obj_set(root, "Step", step);

    /* I18n */
    Value *i18n = val_object();
    val_obj_set(i18n, "Locale",       val_string(ctx->i18n.locale));
    val_obj_set(i18n, "MessagesJSON", i18n_messages_json(ctx->i18n.messages));
    val_obj_set(root, "I18n", i18n);

    /* Features */
    Value *features = val_object();
    Value *feat_enabled = val_object();
    if (ctx->features.enabled) {
        size_t fidx = 0;
        StrMapEntry *fe = NULL;
        while (strmap_iter(ctx->features.enabled, &fidx, &fe)) {
            val_obj_set(feat_enabled, fe->key,
                        val_bool(strcmp(fe->value, "true") == 0));
        }
    }
    val_obj_set(features, "Enabled", feat_enabled);
    val_obj_set(root, "Features", features);

    /* Assets */
    Value *assets = val_object();
    val_obj_set(assets, "BaseURL", val_string(ctx->assets.base_url));
    Value *files = val_object();
    if (ctx->assets.files) {
        size_t aidx = 0;
        StrMapEntry *ae = NULL;
        while (strmap_iter(ctx->assets.files, &aidx, &ae))
            val_obj_set(files, ae->key, val_string(ae->value));
    }
    val_obj_set(assets, "Files", files);
    val_obj_set(root, "Assets", assets);

    return root;
}

void tpl_context_free(TemplateContext *ctx) {
    if (!ctx) return;
    free(ctx->cart.coupon_applied);
    ctx->cart.coupon_applied = NULL;
    if (ctx->i18n.messages) {
        strmap_free(ctx->i18n.messages);
        ctx->i18n.messages = NULL;
    }
    if (ctx->features.enabled) {
        strmap_free(ctx->features.enabled);
        ctx->features.enabled = NULL;
    }
    if (ctx->assets.files) {
        strmap_free(ctx->assets.files);
        ctx->assets.files = NULL;
    }
}
