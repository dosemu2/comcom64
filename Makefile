# Project: FreeDOS-32 command
# Makefile for DJGPP and Mingw32

DOS_CC ?= i586-pc-msdosdjgpp-gcc
DOS_LD ?= i586-pc-msdosdjgpp-gcc
DOS_AS ?= i586-pc-msdosdjgpp-as
DOS_STRIP ?= i586-pc-msdosdjgpp-strip
PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom32
C_OPT = -Wall -O2 -finline-functions -Wmissing-declarations -Wwrite-strings -march=i386
LINK_OPT =
OBJS = command.o cmdbuf.o version.o memmem.o int23.o
CMD = comcom32.exe
RELVER = alpha3
PKG = comcom32-0.1$(RELVER)
TGZ = $(PKG).tar.gz
REVISIONID := $(shell git describe --dirty=+)

.PHONY: all clean

all: $(CMD)

clean:
	$(RM) $(CMD)
	$(RM) *.o

.PHONY: force
version: force
	echo '"$(REVISIONID)"' | cmp -s - "$@" || echo '"$(REVISIONID)"' > "$@"

version.o: version

$(CMD): $(OBJS)
	$(DOS_LD) $^ $(LINK_OPT) -o $(CMD)
	$(DOS_STRIP) $(CMD)
	chmod -x $(CMD)

# Common rules
%.o : %.c
	$(DOS_CC) $(C_OPT) -c $< -o $@

%.o : %.S
	$(DOS_AS) $< -o $@

ifeq (,$(wildcard $(CMD)))
install:
	@echo "Build it first!" && false
else
install:
endif
	install -D -t $(DESTDIR)$(DATADIR) -m 0644 $(CMD)
	ln -sf $(CMD) $(DESTDIR)$(DATADIR)/command.com

uninstall:
	rm -rf $(DATADIR)

$(TGZ):
	git archive -o $(CURDIR)/$(TGZ) --prefix=$(PKG)/ HEAD
.PHONY: $(TGZ)

tar: $(TGZ)

rpm: comcom32.spec.rpkg
	git clean -fd
	rpkg local

deb:
	debuild -i -us -uc -b

checkinstall:
	checkinstall --nodoc -y -D make install PREFIX=/usr
