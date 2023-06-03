SHELL := /bin/bash -o pipefail
HOST_CC = gcc -E -CC -g0 $(CPPFLAGS) $< | gcc -xc -
HOST_LD = gcc
HOST_STRIP ?= strip --strip-debug
DOS_LD = ld -melf_i386 -static
DOS_AS = as --32
PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom32
CPPFLAGS = -I /home/stas/src/djgpp-cvs/include -I .
C_OPT = -Wall -O2 -Wmissing-declarations -Wwrite-strings -fpic -ggdb3
# avoid CMOVxx instructions
#C_OPT += -march=i386
LINK_OPT = ~/src/djgpp-cvs/lib/libdj64.so -shared -Wl,-Bsymbolic
OBJS = command.o cmdbuf.o version.o memmem.o thunks_a.o thunks_c.o
ASOBJS = int23.o plt.o
LIBCMD = comcom32.so
CMD = comcom32.exe
REVISIONID := $(shell git describe --dirty=+)
srcdir ?= .
SRC = $(srcdir)

.PHONY: all clean install uninstall

all: $(CMD)

clean:
	$(RM) $(CMD) $(LIBCMD) *.o version
	$(RM) *.asm *.inc *.tmp thunk_calls.h thunk_asms.h glob_asmdefs.h
	$(MAKE) -C parsers clean

.PHONY: force
version: force
	echo '"$(REVISIONID)"' | cmp -s - "$@" || echo '"$(REVISIONID)"' > "$@"

version.o: version

$(OBJS): $(wildcard *.h)
$(LIBCMD): $(OBJS)
	$(HOST_LD) $^ $(LINK_OPT) -o $@
	$(HOST_STRIP) $@

$(CMD): $(LIBCMD) $(ASOBJS)
	$(DOS_LD) $(ASOBJS) ~/src/djgpp-cvs/lib/libc.a -o $@_
	objcopy -O pe-i386 $@_ $@
	$(RM) $@_
	./djstubify -l $< $@

# Common rules
%.o : %.c glob_asmdefs.h
	$(HOST_CC) $(C_OPT) -c -o $@

%.o : %.S
	$(DOS_CC) -c $< -o $@

plt.o: plt.asm
	$(DOS_AS) -o $@ $<

plt.asm: $(srcdir)/plt.S plt.inc $(SRC)/glob_asm.h
	$(CPP) $(CPPFLAGS) -I . -P $< >$@ || ($(RM) $@ ; false)

thunks_c.o: thunk_calls.h

GEN_TMP = thunk_calls.tmp thunk_asms.tmp plt.inc plt_asmc.inc plt_asmp.inc
_pos = $(if $(findstring $1,$2),$(call _pos,$1,\
       $(wordlist 2,$(words $2),$2),x $3),$3)
pos = $(words $(call _pos,$1,$2))
pars = srcdir=$(srcdir)/parsers \
	$(srcdir)/parsers/parse_decls.sh $(call pos,$@,$(GEN_TMP)) $< >$@ || \
	($(RM) $@ ; false)
$(filter %.tmp,$(GEN_TMP)): $(SRC)/asm.h
	$(pars)
plt.inc: thunk_calls.tmp
	$(pars)
plt_asmc.inc plt_asmp.inc: thunk_asms.tmp
	$(pars)

thunk_calls.h: thunk_calls.tmp parsers/thunk_gen
	parsers/thunk_gen <$< >$@ || ($(RM) $@ ; false)

thunk_asms.h: thunk_asms.tmp parsers/thunk_gen $(srcdir)/parsers/thunks.m4
	$(srcdir)/parsers/gen_thunks.sh $< $(srcdir)/parsers >$@ || \
		($(RM) $@ ; false)

glob_asmdefs.h: $(SRC)/glob_asm.h
	$(srcdir)/parsers/mkasmdefs.sh $< >$@

parsers/thunk_gen: $(srcdir)/parsers/thunk_gen.y $(srcdir)/parsers/thunk_gen.l
	+cd parsers && $(MAKE) srcdir=$(abspath $(srcdir))/parsers

ifeq (,$(wildcard $(CMD)))
install:
	@echo "Build it first or run \"make fetch\"" && false
else
install:
endif
	mkdir -p $(DESTDIR)$(DATADIR)
	install -m 0644 $(CMD) $(DESTDIR)$(DATADIR)
	ln -sf $(CMD) $(DESTDIR)$(DATADIR)/command.com

uninstall:
	rm -rf $(DATADIR)
