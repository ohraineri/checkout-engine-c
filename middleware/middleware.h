#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include "httpd/request.h"
#include "httpd/response.h"

/* Locale middleware: fills req->locale, req->currency from query/headers.
 * default_locale and default_currency are fallbacks. */
void locale_middleware(Request *req, const char *default_locale,
                       const char *default_currency);

/* Cache middleware: sets Cache-Control, Vary, Cache-Tag on resp. */
void cache_middleware(Response *resp, int max_age,
                      const char *store_slug,
                      const char *theme_slug,
                      const char *step,
                      const char *locale,
                      const char *currency);

#endif /* MIDDLEWARE_H */
