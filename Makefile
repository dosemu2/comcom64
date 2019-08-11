# Project: FreeDOS-32 command
# Makefile for DJGPP and Mingw32

DOS_CC ?= i586-pc-msdosdjgpp-gcc
DOS_LD ?= i586-pc-msdosdjgpp-gcc
DOS_STRIP ?= i586-pc-msdosdjgpp-strip
PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom32
C_OPT = -Wall -O2 -finline-functions -Wmissing-declarations
LINK_OPT =
OBJS = command.o cmdbuf.o version.o
CMD = comcom32.exe
RELVER = alpha1
PKG = comcom32-0.1$(RELVER)
TGZ = $(PKG).tar.gz
REVISIONID := "git $(shell git describe --abbrev=12 --always --dirty=+)"

.PHONY: all clean

all: $(CMD)

clean:
	$(RM) $(CMD)
	$(RM) *.o

.PHONY: force
version: force
	echo '$(REVISIONID)' | cmp -s - "$@" || echo '$(REVISIONID)' > "$@"

version.c: version

# hack: make objs intermediate to avoid their rebuild when binary is there
.INTERMEDIATE: $(OBJS)
$(CMD): $(OBJS)
	$(DOS_LD) $(LINK_OPT) $(OBJS) -o $(CMD)
	$(DOS_STRIP) $(CMD)
	chmod -x $(CMD)

# Common rules
%.o : %.c
	$(REDIR) $(DOS_CC) $(C_OPT) $(C_OUTPUT) -c $<

install: $(CMD)
	install -D -t $(DESTDIR)$(DATADIR) -m 0644 $(CMD)
	ln -sf $(CMD) $(DESTDIR)$(DATADIR)/command.com

$(TGZ):
	git archive -o $(CURDIR)/$(TGZ) --prefix=$(PKG)/ HEAD
.PHONY: $(TGZ)

tar: $(TGZ)

rpm: $(TGZ)
	rpmbuild --define "debug_package %{nil}" -tb $(TGZ)

deb:
	debuild -i -us -uc -b
