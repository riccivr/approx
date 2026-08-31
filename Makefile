# approx - non-interactive fuzzy stream filter
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
	@mkdir -p approx-$(VERSION)/tests approx-$(VERSION)/examples
	@cp -R LICENSE Makefile README.md config.mk approx.1 arg.h approx.h approx.c tests examples approx-$(VERSION)
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

test-posix: approx
	@sh tests/test_posix.sh

test-stress: approx
	@sh tests/test_stress.sh

test-properties: approx
	@sh tests/test_properties.sh

test-all: test test-posix test-stress test-properties test-examples test-cpp

test-valgrind: clean
	@echo building with debug symbols
	@$(CC) $(CFLAGS) -g approx.c -o approx $(LDFLAGS)
	@echo running valgrind leak check
	@printf "match test line\n" | valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./approx "test" >/dev/null
	@printf "apple\nrecieve\n" | valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./approx -n 2 -s "receive" >/dev/null
	@echo valgrind: 0 memory leaks, 0 errors

test-tcc: clean
	@echo compiling with tcc
	@tcc -std=c99 -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\" approx.c -o approx -lm
	@sh tests/test_approx.sh
	@sh tests/test_posix.sh

test-clang: clean
	@echo compiling with clang
	@clang -std=c99 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\" approx.c -o approx -lm
	@sh tests/test_approx.sh
	@sh tests/test_posix.sh

sanitize: clean
	@echo CC -fsanitize=address,undefined -o approx
	@$(CC) $(CFLAGS) -g -fsanitize=address,undefined approx.c -o approx $(LDFLAGS) -fsanitize=address,undefined
	@sh tests/test_approx.sh
	@sh tests/test_posix.sh
	@sh tests/test_stress.sh
	@sh tests/test_properties.sh

test-cpp: clean
	@echo compiling C++ test
	@$${CXX:-c++} -std=c++11 -Wall -Wextra -pedantic -I. examples/embed_demo_cpp.cpp -o examples/embed_demo_cpp
	@./examples/embed_demo_cpp >/dev/null
	@rm -f examples/embed_demo_cpp
	@echo "C++ embedding test passed"

test-examples: clean
	@echo compiling C example
	@$(CC) $(CFLAGS) -I. examples/embed_demo.c -o examples/embed_demo $(LDFLAGS)
	@./examples/embed_demo >/dev/null
	@rm -f examples/embed_demo
	@echo "C embedding test passed"

bench: approx
	@sh tests/benchmark.sh

.PHONY: all options clean dist install uninstall test test-posix test-stress test-properties test-all test-valgrind test-tcc test-clang test-cpp test-examples sanitize bench
