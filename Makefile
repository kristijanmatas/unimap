PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
MANDIR  ?= $(PREFIX)/share/man/man1

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -Wall -Wextra

all: unimap

unimap: unimap.c unames.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ unimap.c

static: unimap.c unames.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -static -s -o unimap unimap.c

install: unimap
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)
	install -m 755 unimap $(DESTDIR)$(BINDIR)/unimap
	install -m 644 unimap.1 $(DESTDIR)$(MANDIR)/unimap.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/unimap $(DESTDIR)$(MANDIR)/unimap.1

deb:
	dpkg-buildpackage -us -uc -b

clean:
	rm -f unimap

.PHONY: all static install uninstall deb clean
