#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "config/config.h"
#include "engine/theme_loader.h"
#include "engine/i18n_loader.h"
#include "engine/template_cache.h"
#include "engine/renderer.h"
#include "handlers/checkout.h"
#include "httpd/server.h"

int main(void) {
    Config cfg;
    config_load(&cfg);

    char addr[256];
    config_addr(&cfg, addr, sizeof(addr));
    printf("Config loaded, addr=%s\n", addr);

    /* Load themes */
    ThemeLoader *theme_loader = theme_loader_new(cfg.themes.root_path);
    if (!theme_loader) {
        fprintf(stderr, "OOM: theme_loader_new\n");
        return 1;
    }
    printf("Loading themes from %s\n", cfg.themes.root_path);
    if (theme_loader_load_all(theme_loader) != 0) {
        fprintf(stderr, "Failed to load themes\n");
        return 1;
    }
    {
        char slugs[MAX_THEMES][MAX_THEME_SLUG];
        int n = theme_loader_themes(theme_loader, slugs, MAX_THEMES);
        printf("Themes loaded: %d\n", n);
    }

    /* I18n loader */
    I18nLoader *i18n_loader = i18n_loader_new(theme_loader);
    if (!i18n_loader) {
        fprintf(stderr, "OOM: i18n_loader_new\n");
        return 1;
    }

    /* Template cache */
    TemplateCache *tpl_cache = template_cache_new(theme_loader);
    if (!tpl_cache) {
        fprintf(stderr, "OOM: template_cache_new\n");
        return 1;
    }

    /* Warm up cache */
    if (cfg.cache.warm_up_on_boot) {
        const char *warmup_steps[] = {
            "address", "shipping", "payment", "one-click", "multi-step"
        };
        int nsteps = 5;

        char slugs[MAX_THEMES][MAX_THEME_SLUG];
        int nthemes = theme_loader_themes(theme_loader, slugs, MAX_THEMES);
        for (int i = 0; i < nthemes; i++) {
            printf("Warming up theme: %s\n", slugs[i]);
            template_cache_warmup(tpl_cache, slugs[i], warmup_steps, nsteps);
        }
        printf("Warm-up complete\n");
    }

    /* Renderer */
    Renderer *renderer = renderer_new(theme_loader, tpl_cache);
    if (!renderer) {
        fprintf(stderr, "OOM: renderer_new\n");
        return 1;
    }

    /* Checkout handler */
    CheckoutHandler *checkout_handler = checkout_handler_new(
        renderer,
        theme_loader,
        i18n_loader,
        cfg.defaults.locale,
        cfg.defaults.currency,
        cfg.cache.shell_max_age
    );
    if (!checkout_handler) {
        fprintf(stderr, "OOM: checkout_handler_new\n");
        return 1;
    }

    /* HTTP server */
    Server server;
    server_init(&server, cfg.themes.root_path);

    checkout_handler_register(checkout_handler, &server);

    printf("Server listening on %s:%s\n", cfg.server.host, cfg.server.port);

    int rc = server_listen_and_serve(&server, cfg.server.host, cfg.server.port);

    /* Cleanup (unreachable in normal operation) */
    checkout_handler_free(checkout_handler);
    renderer_free(renderer);
    template_cache_free(tpl_cache);
    i18n_loader_free(i18n_loader);
    theme_loader_free(theme_loader);

    return rc;
}
