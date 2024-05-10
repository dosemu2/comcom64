AS = $(CROSS_PREFIX)as
CROSS_PREFIX := i686-linux-gnu-
ifeq ($(shell $(AS) --version 2>/dev/null),)
CROSS_PREFIX := x86_64-linux-gnu-
endif
ifeq ($(shell $(AS) --version 2>/dev/null),)
ifeq ($(shell uname -m),x86_64)
CROSS_PREFIX :=
else
$(error cross-binutils not installed)
endif
endif

HOST_CC = dj64-gcc
HOST_LD = gcc
HOST_AS = gcc -E -x assembler-with-cpp $< | $(CROSS_PREFIX)as --32
HOST_STRIP ?= strip
DOS_LD = $(CROSS_PREFIX)ld -melf_i386 -static
DOS_STRIP = $(CROSS_PREFIX)strip --strip-debug
PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom64
C_OPT = -Wall -Og -Wmissing-declarations -Wwrite-strings -fpic \
  -ggdb3 -I. -Wunused -Wmissing-prototypes
LINK_OPT = $(shell pkg-config --libs dj64) -shared -Wl,-Bsymbolic \
  -Wl,-rpath=/usr/local/i386-pc-dj64/lib64 \
  -Wl,-rpath=/usr/i386-pc-dj64/lib64
DOSLDFLAGS = --whole-archive $(shell pkg-config --static --libs dj64static)
CPPFLAGS = -I. $(shell pkg-config --cflags dj64)
SRCS = command.c cmdbuf.c ms.c env.c psp.c umb.c ae0x.c compl.c \
  thunks_a.c thunks_c.c
OBJS = $(SRCS:.c=.o)
ASSRCS = int23.S int0.S asm.S plt.S mouse.S
ASOBJS = $(ASSRCS:.S=.o)
LIBCMD = libcomcom64.so
CMD = comcom64.exe
ELF = comcom64.elf
DBG = dosemu_$(CMD).dbg
FLG = $(shell objdump -T $(LIBCMD) | grep _shm_flags | sed -E 's/([^ ]+) .+/0x\1/')
REVISIONID := $(shell git describe --dirty=+)
C_OPT += -DREV_ID=\"$(REVISIONID)\"
srcdir ?= .
SRC = $(srcdir)
TG = $(shell pkg-config --variable=binary thunk_gen)
ifeq ($(TG),)
ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error thunk_gen not installed)
endif
endif
TGS = $(shell pkg-config --variable=tgscript thunk_gen)
PDS = $(shell pkg-config --variable=pdscript thunk_gen)
TGM4 = $(shell pkg-config --variable=m4script thunk_gen)
TFLAGS = -a 4 -p 4

.PHONY: all clean install uninstall

all: $(CMD)

clean:
	$(RM) $(CMD) $(LIBCMD) $(ELF) *.dbg *.o version
	$(RM) *.asm plt.inc *.tmp thunk_calls.h thunk_asms.h

$(OBJS): $(wildcard *.h)
$(LIBCMD): $(OBJS)
	$(HOST_LD) $^ $(LINK_OPT) -o $@
	objcopy --only-keep-debug $@ $(DBG) || $(RM) $@
	$(HOST_STRIP) $@ || $(RM) $@
	objcopy --add-gnu-debuglink=$(DBG) $@ || $(RM) $@

$(ELF): $(ASOBJS)
	$(DOS_LD) $^ $(DOSLDFLAGS) -o $@
	$(DOS_STRIP) $@

$(CMD): $(LIBCMD) $(ELF)
	djstubify -l $< -l $(DBG) -n $(CMD) -f $(FLG) -o $@ $(ELF)

#.INTERMEDIATE: $(LIBCMD)

# Common rules
%.o : %.c
	$(HOST_CC) $(C_OPT) -o $@ -c $<

%.o : %.S
	$(HOST_AS) -o $@

plt.o: plt.asm
	$(HOST_AS) -o $@

plt.asm: $(srcdir)/plt.S plt.inc $(SRC)/glob_asm.h
	$(CPP) $(CPPFLAGS) -P $< >$@ || ($(RM) $@ ; false)

thunks_c.o: thunk_calls.h

GEN_TMP = thunk_calls.tmp thunk_asms.tmp plt.inc plt_asmc.h plt_asmp.h
_pos = $(if $(findstring $1,$2),$(call _pos,$1,\
       $(wordlist 2,$(words $2),$2),x $3),$3)
pos = $(words $(call _pos,$1,$2))
pars = $(PDS) $(call pos,$@,$(GEN_TMP)) $< >$@ || ($(RM) $@ ; false)
$(filter %.tmp,$(GEN_TMP)): $(SRC)/asm.h
	$(pars)
plt.inc: thunk_calls.tmp
	$(pars)
plt_asmc.h plt_asmp.h: thunk_asms.tmp
	$(pars)

thunk_calls.h: thunk_calls.tmp
	$(TG) $(TFLAGS) <$< >$@ || ($(RM) $@ ; false)

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
