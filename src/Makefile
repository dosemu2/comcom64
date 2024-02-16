HOST_CC = dj64-gcc
HOST_LD = gcc
HOST_AS = gcc -m32 -x assembler-with-cpp
HOST_STRIP ?= strip
DOS_LD = ld -melf_i386 -static
DOS_STRIP = strip --strip-debug
PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom64
C_OPT = -Wall -Werror -O2 -Wmissing-declarations -Wwrite-strings -fpic \
  -ggdb3 -I.
LINK_OPT = $(shell pkg-config --libs dj64) -shared -Wl,-Bsymbolic \
  -Wl,-rpath=/usr/local/i386-pc-dj64/lib64 \
  -Wl,-rpath=/usr/i386-pc-dj64/lib64
OBJS = command.o cmdbuf.o version.o thunks_a.o thunks_c.o
ASOBJS = int23.o int0.o asm.o plt.o
LIBCMD = libcomcom64.so
CMD = comcom64.exe
ELF = comcom64.elf
DBG = dosemu_$(CMD).dbg
REVISIONID := $(shell git describe --dirty=+)
srcdir ?= .
SRC = $(srcdir)
TG = $(shell pkg-config --variable=binary thunk_gen)
ifeq ($(TG),)
ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error thunk_gen not installed)
endif
endif
TGS = $(shell pkg-config --variable=tgscript thunk_gen)
TGM4 = $(shell pkg-config --variable=m4script thunk_gen)
TFLAGS = -a 4 -p 4

.PHONY: all clean install uninstall

all: $(CMD)

clean:
	$(RM) $(CMD) $(LIBCMD) $(ELF) *.dbg *.o version
	$(RM) *.asm plt.inc *.tmp thunk_calls.h thunk_asms.h glob_asmdefs.h

.PHONY: force
version: force
	echo '"$(REVISIONID)"' | cmp -s - "$@" || echo '"$(REVISIONID)"' > "$@"

version.o: version

$(OBJS): $(wildcard *.h)
$(LIBCMD): $(OBJS)
	$(HOST_LD) $^ $(LINK_OPT) -o $@
	objcopy --only-keep-debug $@ $(DBG) || $(RM) $@
	$(HOST_STRIP) $@ || $(RM) $@
	objcopy --add-gnu-debuglink=$(DBG) $@ || $(RM) $@

$(ELF): $(ASOBJS)
	$(DOS_LD) $(ASOBJS) --whole-archive $(shell pkg-config --static --libs dj64static) -o $@
	$(DOS_STRIP) $@

$(CMD): $(LIBCMD) $(ELF)
	djstubify -l $< -l $(DBG) -o $@ $(ELF)

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

thunk_calls.h: thunk_calls.tmp
	$(TG) $(TFLAGS) <$< >$@ || ($(RM) $@ ; false)

glob_asmdefs.h: $(SRC)/glob_asm.h
	$(srcdir)/parsers/mkasmdefs.sh $< >$@

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
