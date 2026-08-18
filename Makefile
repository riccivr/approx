# approx - non-interactive POSIX fuzzy stream filter and ranker
# See LICENSE file for copyright and license details.

include config.mk

SRC = approx.c
OBJ = $(SRC:.c=.o)

all: approx

.c.o:
	$(CC) -c $(CFLAGS) $<

approx: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f approx $(OBJ) approx-$(VERSION).tar.gz

dist: clean
	mkdir -p approx-$(VERSION)/tests
	cp -R LICENSE Makefile README.md config.mk approx.1 arg.h approx.h approx.c tests approx-$(VERSION)
	tar -cf approx-$(VERSION).tar approx-$(VERSION)
	gzip approx-$(VERSION).tar
	rm -rf approx-$(VERSION)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f approx $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/approx
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < approx.1 > $(DESTDIR)$(MANPREFIX)/man1/approx.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/approx.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/approx
	rm -f $(DESTDIR)$(MANPREFIX)/man1/approx.1

test: approx
	sh tests/test_approx.sh

sanitize: clean
	$(CC) $(CFLAGS) -g -fsanitize=address,undefined approx.c -o approx $(LDFLAGS) -fsanitize=address,undefined
	sh tests/test_approx.sh

.PHONY: all clean dist install uninstall test sanitize
