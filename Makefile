ifndef CC
CC = gcc
endif

PACKAGENAME     = deepin-debug-config
LIBSO           = libdbgconfig.so
DESTDIR         ?= /
PREFIX          ?= /usr
BINDIR          = $(PREFIX)/bin
LIBDIR          = $(PREFIX)/lib/$(DEB_HOST_MULTIARCH)
MANDIR          = $(PREFIX)/share/man
INSTALL         ?= install -p
LANGUAGES       = $(shell cat po/LINGUAS | xargs)

GLIB_LIBS       = $(shell pkg-config --libs glib-2.0)
GLIB_CFLAGS     = $(shell pkg-config --cflags glib-2.0)
SYSTEMD_LIBS    = $(shell pkg-config --libs libsystemd)
SYSTEMD_CFLAGS  = $(shell pkg-config --cflags libsystemd)

CFLAGS += -MMD -O2 -Wall -g -fPIC $(GLIB_CFLAGS) $(SYSTEMD_CFLAGS)
LDFLAGS += -L. $(GLIB_LIBS) $(SYSTEMD_LIBS) -lcrypto

# 源文件列表（排除 generate_sha256.c）
LIB_SRCS := cJSON.c module_configure.c util.c
LIB_OBJS := $(patsubst %.c, %.o, $(LIB_SRCS))

-include $(LIB_OBJS:.o=.d)

all: $(PACKAGENAME) deepin-debug-config-service translate

$(PACKAGENAME): main.o $(LIBSO)
	$(CC) -o $@ $^ -L. -ldbgconfig

deepin-debug-config-service: bus-service.o $(LIBSO)
	$(CC) -o $@ $^ -L. -ldbgconfig $(SYSTEMD_LIBS)

$(LIBSO): $(LIB_OBJS) config_sha256.o
	$(CC) -shared -o $@ $^ $(LDFLAGS)

config_sha256.c: generate_sha256
	./generate_sha256

generate_sha256: generate_sha256.c util.c
	$(CC) -o $@ $^ -lcrypto

clean:
	rm -f $(PACKAGENAME) deepin-debug-config-service $(LIBSO) *.o *.d generate_sha256 config_sha256.c config_sha256.o
	rm -rf out/locale

install: all
	mkdir -pv $(DESTDIR)$(BINDIR)
	$(INSTALL) -s -m 755 $(PACKAGENAME) $(DESTDIR)$(BINDIR)/
	$(INSTALL) -s -m 755 deepin-debug-config-service $(DESTDIR)$(BINDIR)/
	mkdir -pv $(DESTDIR)$(LIBDIR)
	$(INSTALL) -s -m 644 $(LIBSO) $(DESTDIR)$(LIBDIR)/
	mkdir -pv $(DESTDIR)$(PREFIX)/share/deepin-debug-config/deepin-debug-config.d
	$(INSTALL) -s -m 644 out/deepin-debug-config/deepin-debug-config.d/*.json $(DESTDIR)$(PREFIX)/share/deepin-debug-config/deepin-debug-config.d/
	$(INSTALL) -s -m 644 out/deepin-debug-config/deepin-debug-config.d/*.conf $(DESTDIR)$(PREFIX)/share/deepin-debug-config/deepin-debug-config.d/
	$(INSTALL) -s -m 644 out/deepin-debug-config/deepin-debug-config.d/dbg.sources.list $(DESTDIR)$(PREFIX)/share/deepin-debug-config/deepin-debug-config.d/sources.list
	mkdir -pv $(DESTDIR)$(PREFIX)/share/deepin-debug-config/shell
	$(INSTALL) -s -m 755 out/deepin-debug-config/shell/*.sh $(DESTDIR)$(PREFIX)/share/deepin-debug-config/shell/
	mkdir -pv $(DESTDIR)$(PREFIX)/share/locale
	cp -rv out/locale/* $(DESTDIR)$(PREFIX)/share/locale/
	mkdir -pv $(DESTDIR)/var/lib/deepin-debug-config/lists/
	mkdir -pv $(DESTDIR)/var/cache/deepin-debug-config/
	mkdir -pv $(DESTDIR)/etc/systemd/journald.conf.d/

pot:
	xgettext --default-domain=$(PACKAGENAME) --directory=. --keyword=_ --keyword=N_ --no-location --files-from=./po/POTFILES.in --output-dir=./po/
	mv po/$(PACKAGENAME).po po/$(PACKAGENAME).pot
	sed -i 's/charset=CHARSET/charset=UTF-8/g' po/$(PACKAGENAME).pot

po-update: pot
	@for lang in $(LANGUAGES); do \
		if test -f po/$$lang.po; then \
			msgmerge --previous --update po/$$lang.po po/$(PACKAGENAME).pot; \
		else \
			msginit --input=po/$(PACKAGENAME).pot --locale=$$lang --output-file=po/$$lang.po; \
		fi \
	done

out/locale/%/LC_MESSAGES/$(PACKAGENAME).mo: po/%.po
	mkdir -p $(@D)
	msgmerge --previous --update $< po/$(PACKAGENAME).pot
	msgfmt -o $@ $<

translate: $(addsuffix /LC_MESSAGES/$(PACKAGENAME).mo, $(addprefix out/locale/, $(LANGUAGES)))

.PHONY: all clean install pot po-update translate
