#include "middleware/middleware.h"

#include <stdio.h>
#include <string.h>

/* Extract first language tag from Accept-Language: en-US,en;q=0.9,... */
static void first_accept_language(const char *s, char *out, int outsz) {
    if (!s || !s[0]) { out[0] = '\0'; return; }
    /* Find first comma */
    const char *comma = strchr(s, ',');
    char first[128];
    if (comma) {
        int n = (int)(comma - s);
        if (n >= 128) n = 127;
        memcpy(first, s, (size_t)n);
        first[n] = '\0';
    } else {
        snprintf(first, sizeof(first), "%s", s);
    }
    /* Strip ;q=... */
    char *semi = strchr(first, ';');
    if (semi) *semi = '\0';
    /* Trim */
    char *p = first;
    while (*p == ' ') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t')) p[--len] = '\0';

    snprintf(out, outsz, "%s", p);
}

void locale_middleware(Request *req, const char *default_locale,
                       const char *default_currency) {
    /* Locale: ?locale= > Accept-Language > default */
    const char *locale = request_query(req, "locale");
    if (!locale || !locale[0]) {
        const char *al = request_header(req, "Accept-Language");
        char buf[64] = "";
        first_accept_language(al, buf, sizeof(buf));
        if (buf[0]) {
            snprintf(req->locale, sizeof(req->locale), "%s", buf);
        } else {
            snprintf(req->locale, sizeof(req->locale), "%s", default_locale);
        }
    } else {
        snprintf(req->locale, sizeof(req->locale), "%s", locale);
    }

    /* Currency: ?currency= > X-Currency > default */
    const char *currency = request_query(req, "currency");
    if (!currency || !currency[0]) {
        currency = request_header(req, "X-Currency");
        if (!currency || !currency[0]) {
            snprintf(req->currency, sizeof(req->currency), "%s", default_currency);
        } else {
            snprintf(req->currency, sizeof(req->currency), "%s", currency);
        }
    } else {
        snprintf(req->currency, sizeof(req->currency), "%s", currency);
    }
}
