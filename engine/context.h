#ifndef ENGINE_CONTEXT_H
#define ENGINE_CONTEXT_H

#include <stdint.h>
#include "engine/value.h"
#include "util/strmap.h"

/* ---------- Plain C structs mirroring Go models ---------- */

typedef struct {
    int64_t amount;
    char    currency[16];
    char    display[64];
} Money;

typedef struct {
    char   id[64];
    char   name[256];
    char   image_url[512];
    int    quantity;
    Money  unit_price;
    Money  total;
} CartItem;

#define MAX_CART_ITEMS 64

typedef struct {
    CartItem items[MAX_CART_ITEMS];
    int      item_count_arr;  /* number of items in array */
    Money    subtotal;
    Money    shipping;
    Money    discount;
    Money    total;
    char    *coupon_applied;  /* NULL = not applied */
    int      item_count;
} CartData;

typedef struct {
    char id[128];
    char customer_name[256];
    char email[256];
    int  is_guest;
} SessionData;

typedef struct {
    char primary_color[32];
    char secondary_color[32];
    char font_family[128];
    char border_radius[32];
} DesignTokens;

typedef struct {
    char         slug[128];
    char         name[256];
    char         logo_url[512];
    char         theme_slug[128];
    char         flow_profile[64];
    char         locale[64];
    char         currency[16];
    DesignTokens tokens;
} StoreConfig;

#define MAX_STEPS 16

typedef struct {
    char    current[64];
    int     total;
    char    steps[MAX_STEPS][64];
    int     step_count;
    int     progress;
} StepState;

typedef struct {
    char     locale[64];
    StrMap  *messages;  /* owned */
} I18nBundle;

typedef struct {
    StrMap *enabled;   /* key -> "true"/"false" */
} FeatureFlags;

typedef struct {
    char    base_url[512];
    StrMap *files;     /* filename -> path */
} AssetManifest;

typedef struct {
    SessionData  session;
    CartData     cart;
    StoreConfig  store;
    StepState    step;
    I18nBundle   i18n;
    FeatureFlags features;
    AssetManifest assets;
} TemplateContext;

/* ---------- Currency formatting ---------- */
void format_currency(int64_t cents, const char *code, char *out, int outsz);

/* ---------- Build a Value tree from a TemplateContext ---------- */
/* The returned Value* must be freed with val_free() */
Value *build_template_value(const TemplateContext *ctx);

/* ---------- Free resources inside TemplateContext ---------- */
void tpl_context_free(TemplateContext *ctx);

#endif /* ENGINE_CONTEXT_H */
