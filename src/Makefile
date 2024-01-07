SHELL := /bin/bash -o pipefail
HOST_CC = /usr/i386-pc-dj64/bin/ccwrp.sh
HOST_LD = gcc
HOST_AS = gcc -m32 -x assembler-with-cpp
HOST_STRIP ?= strip --strip-debug
DOS_LD = ld -melf_i386 -static
DOS_STRIP = strip
PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom64
C_OPT = -Wall -O2 -Wmissing-declarations -Wwrite-strings -fpic -ggdb3 -I.
LINK_OPT = /usr/i386-pc-dj64/lib64/libdj64.so -shared -Wl,-Bsymbolic
OBJS = command.o cmdbuf.o version.o thunks_a.o thunks_c.o
ASOBJS = int23.o int0.o asm.o plt.o
LIBCMD = libcomcom64.so
CMD = comcom64.exe
ELF = comcom64.elf
REVISIONID := $(shell git describe --dirty=+)
srcdir ?= .
SRC = $(srcdir)
TFLAGS = -a 4 -p 4

.PHONY: all clean install uninstall

all: $(CMD)

clean:
	$(RM) $(CMD) $(LIBCMD) $(ELF) *.o version
	$(RM) *.asm plt.inc *.tmp thunk_calls.h thunk_asms.h glob_asmdefs.h
	$(MAKE) -C thunk_gen clean

.PHONY: force
version: force
	echo '"$(REVISIONID)"' | cmp -s - "$@" || echo '"$(REVISIONID)"' > "$@"

version.o: version

$(OBJS): $(wildcard *.h)
$(LIBCMD): $(OBJS)
	$(HOST_LD) $^ $(LINK_OPT) -o $@
	$(HOST_STRIP) $@

$(ELF): $(ASOBJS)
	$(DOS_LD) $(ASOBJS) /usr/i386-pc-dj64/lib/libc.a -o $@
	$(DOS_STRIP) $@

$(CMD): $(LIBCMD) $(ELF)
	djstubify -l $< -o $@ $(ELF)

#.INTERMEDIATE: $(LIBCMD)

# Common rules
%.o : %.c glob_asmdefs.h
	$(HOST_CC) $(C_OPT) -o $@ -c $<

%.o : %.S
	$(HOST_AS) -c $< -o $@

plt.o: plt.asm
	$(HOST_AS) -c -o $@ $<

plt.asm: $(srcdir)/plt.S plt.inc $(SRC)/glob_asm.h
	$(CPP) -I . -P $< >$@ || ($(RM) $@ ; false)

thunks_c.o: thunk_calls.h

GEN_TMP = thunk_calls.tmp thunk_asms.tmp plt.inc plt_asmc.h plt_asmp.h
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
plt_asmc.h plt_asmp.h: thunk_asms.tmp
	$(pars)

thunk_calls.h: thunk_calls.tmp thunk_gen/thunk_gen
	thunk_gen/thunk_gen $(TFLAGS) <$< >$@ || ($(RM) $@ ; false)

thunk_asms.h: thunk_asms.tmp thunk_gen/thunk_gen $(srcdir)/parsers/thunks.m4
	thunk_gen/thunk_gen $(TFLAGS) 1 <$< | sort | uniq | autom4te -l m4sugar $(srcdir)/parsers/thunks.m4 - >$@_ \
		|| ($(RM) $@_ ; false)
	thunk_gen/thunk_gen $(TFLAGS) 2 <$< >$@__ \
		|| ($(RM) $@__ ; false)
	cat $@_ $@__ >$@
	rm -f $@_ $@__

glob_asmdefs.h: $(SRC)/glob_asm.h
	$(srcdir)/parsers/mkasmdefs.sh $< >$@

thunk_gen/thunk_gen: $(srcdir)/thunk_gen/thunk_gen.y $(srcdir)/thunk_gen/thunk_gen.l
	+cd thunk_gen && $(MAKE) srcdir=$(abspath $(srcdir))/thunk_gen

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
