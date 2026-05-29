#include "middleware/middleware.h"

#include <stdio.h>
#include <string.h>

void cache_middleware(Response *resp, int max_age,
                      const char *store_slug,
                      const char *theme_slug,
                      const char *step,
                      const char *locale,
                      const char *currency) {
    /* Suppress unused parameter warnings */
    (void)locale;
    (void)currency;

    char cc[128];
    snprintf(cc, sizeof(cc),
             "public, s-maxage=%d, stale-while-revalidate=60", max_age);
    response_set_header(resp, "Cache-Control", cc);

    response_set_header(resp, "Vary",
                        "Accept-Language, X-Currency, X-AB-Variant");

    /* Cache-Tag: storeSlug themeSlug storeSlug:step */
    char tags[512] = "";
    if (store_slug && store_slug[0]) {
        snprintf(tags, sizeof(tags), "%s", store_slug);
        if (theme_slug && theme_slug[0]) {
            char tmp[256];
            snprintf(tmp, sizeof(tmp), " %s", theme_slug);
            strncat(tags, tmp, sizeof(tags) - strlen(tags) - 1);
        }
        if (step && step[0]) {
            char tmp[256];
            snprintf(tmp, sizeof(tmp), " %s:%s", store_slug, step);
            strncat(tags, tmp, sizeof(tags) - strlen(tags) - 1);
        }
    }
    if (tags[0]) response_set_header(resp, "Cache-Tag", tags);
}
