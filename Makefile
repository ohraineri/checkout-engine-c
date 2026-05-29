CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2 -I. -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lpthread -lcrypto

SRCS = main.c \
       util/buf.c \
       util/strmap.c \
       config/config.c \
       engine/value.c \
       engine/context.c \
       engine/theme_loader.c \
       engine/i18n_loader.c \
       engine/tpl_engine.c \
       engine/template_cache.c \
       engine/renderer.c \
       middleware/locale.c \
       middleware/cache.c \
       handlers/checkout.c \
       httpd/server.c \
       httpd/request.c \
       httpd/response.c

OBJS = $(SRCS:.c=.o)
TARGET = checkout-ssr-c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
