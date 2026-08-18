# approx - non-interactive POSIX fuzzy stream filter and ranker
# See LICENSE file for copyright and license details.

include config.mk

SRC = approx.c
OBJ = $(SRC:.c=.o)

all: options approx

options:
	@echo approx build options:
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CC       = $(CC)"

.c.o:
	@echo CC $<
	@$(CC) -c $(CFLAGS) $<

$(OBJ): config.mk arg.h approx.h

approx: $(OBJ)
	@echo CC -o $@
	@$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	@echo cleaning
	@rm -f approx $(OBJ) approx-$(VERSION).tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p approx-$(VERSION)/tests
	@cp -R LICENSE Makefile README.md config.mk approx.1 arg.h approx.h approx.c tests approx-$(VERSION)
	@tar -cf approx-$(VERSION).tar approx-$(VERSION)
	@gzip approx-$(VERSION).tar
	@rm -rf approx-$(VERSION)

install: all
	@echo installing executable file to $(DESTDIR)$(PREFIX)/bin
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp -f approx $(DESTDIR)$(PREFIX)/bin
	@chmod 755 $(DESTDIR)$(PREFIX)/bin/approx
	@echo installing manual page to $(DESTDIR)$(MANPREFIX)/man1
	@mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	@sed "s/VERSION/$(VERSION)/g" < approx.1 > $(DESTDIR)$(MANPREFIX)/man1/approx.1
	@chmod 644 $(DESTDIR)$(MANPREFIX)/man1/approx.1

uninstall:
	@echo removing executable file from $(DESTDIR)$(PREFIX)/bin
	@rm -f $(DESTDIR)$(PREFIX)/bin/approx
	@echo removing manual page from $(DESTDIR)$(MANPREFIX)/man1
	@rm -f $(DESTDIR)$(MANPREFIX)/man1/approx.1

test: approx
	@sh tests/test_approx.sh

sanitize: clean
	@echo CC -fsanitize=address,undefined -o approx
	@$(CC) $(CFLAGS) -g -fsanitize=address,undefined approx.c -o approx $(LDFLAGS) -fsanitize=address,undefined
	@sh tests/test_approx.sh

.PHONY: all options clean dist install uninstall test sanitize
