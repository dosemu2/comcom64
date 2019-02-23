# Project: FreeDOS-32 command
# Makefile for DJGPP and Mingw32

CC = i586-pc-msdosdjgpp-gcc
STRIP = i586-pc-msdosdjgpp-strip
PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom32
C_OPT = -Wall -O2 -finline-functions -Wmissing-declarations
LINK_OPT =
OBJS = command.o cmdbuf.o
CMD = comcom32.exe
RELVER = alpha1
PKG = comcom32-0.1$(RELVER)
TGZ = $(PKG).tar.gz

.PHONY: all clean

all: $(CMD)

clean:
	$(RM) $(CMD)
	$(RM) *.o

$(CMD): $(OBJS)
	$(CC) $(LINK_OPT) $(OBJS) -o $(CMD)
	$(STRIP) $(CMD)
	chmod -x $(CMD)

# Common rules
%.o : %.c
	$(REDIR) $(CC) $(C_OPT) $(C_OUTPUT) -c $<

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
