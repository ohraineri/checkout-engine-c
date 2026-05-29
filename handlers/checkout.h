#ifndef HANDLERS_CHECKOUT_H
#define HANDLERS_CHECKOUT_H

#include "engine/renderer.h"
#include "engine/theme_loader.h"
#include "engine/i18n_loader.h"
#include "httpd/server.h"

typedef struct {
    Renderer   *renderer;
    ThemeLoader *loader;
    I18nLoader  *i18n_loader;
    char         default_locale[64];
    char         default_currency[16];
    int          cache_max_age;
} CheckoutHandler;

CheckoutHandler *checkout_handler_new(Renderer *renderer,
                                       ThemeLoader *loader,
                                       I18nLoader *i18n_loader,
                                       const char *default_locale,
                                       const char *default_currency,
                                       int cache_max_age);

void checkout_handler_free(CheckoutHandler *h);

/* Register routes on server */
void checkout_handler_register(CheckoutHandler *h, Server *s);

#endif /* HANDLERS_CHECKOUT_H */
