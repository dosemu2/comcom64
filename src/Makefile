DOS_CC ?= i586-pc-msdosdjgpp-gcc
DOS_LD ?= i586-pc-msdosdjgpp-gcc
DOS_AS ?= i586-pc-msdosdjgpp-as
DOS_STRIP ?= i586-pc-msdosdjgpp-strip
NASM = nasm
PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom32
C_OPT = -Wall -O2 -Wmissing-declarations -Wwrite-strings
# avoid CMOVxx instructions
C_OPT += -march=i386
C_OPT += -I .
LINK_OPT =
OBJS = command.o cmdbuf.o version.o memmem.o fmemcpy.o thunks_a.o
CMD = comcom32.exe
REVISIONID := $(shell git describe --dirty=+)
srcdir ?= .
SRC = $(srcdir)
NASMFLAGS += -i$(SRC)/ -f coff

.PHONY: all clean install uninstall

all: $(CMD)

clean:
	$(RM) $(CMD) *.o version
	$(RM) *.asm *.inc *.tmp thunk_calls.h thunk_asms.h glob_asmdefs.h

.PHONY: force
version: force
	echo '"$(REVISIONID)"' | cmp -s - "$@" || echo '"$(REVISIONID)"' > "$@"

version.o: version

$(OBJS): $(wildcard *.h)
$(CMD): $(OBJS)
	$(DOS_LD) $^ $(LINK_OPT) -o $(CMD)
	$(DOS_STRIP) $(CMD)
	chmod -x $(CMD)

# Common rules
%.o : %.c glob_asmdefs.h
	$(DOS_CC) $(C_OPT) -c $< -o $@

%.o : %.S
	$(DOS_CC) -c $< -o $@

plt.o: plt.asm plt.inc
	$(NASM) $(NASMFLAGS) -o $@ $<

plt.asm: $(srcdir)/plt.S $(SRC)/glob_asm.h
	$(CPP) $(CPPFLAGS) -I . -P $< >$@ || ($(RM) $@ ; false)

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
