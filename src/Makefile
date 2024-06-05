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
DOS_LD = $(CROSS_PREFIX)ld -melf_i386 -static
DOS_STRIP = $(CROSS_PREFIX)strip --strip-debug
PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom64
C_OPT = -Wall -Og -Wmissing-declarations -Wwrite-strings -fpic \
  -ggdb3 -I. -Wunused -Wmissing-prototypes
LINK_OPT = $(shell pkg-config --libs dj64) -shared -Wl,-Bsymbolic \
  -Wl,-rpath=/usr/local/i386-pc-dj64/lib64 \
  -Wl,-rpath=/usr/i386-pc-dj64/lib64
DOSLDFLAGS = $(shell pkg-config --static --libs dj64static)
CPPFLAGS = -I. $(shell pkg-config --cflags dj64)
SRCS = command.c cmdbuf.c ms.c env.c psp.c umb.c ae0x.c compl.c \
  thunks_a.c thunks_c.c
HDRS = ae0x.h cmdbuf.h compl.h psp.h command.h env.h ms.h umb.h \
  glob_asm.h asm.h
OBJS = $(SRCS:.c=.o)
ASSRCS = int23.S int0.S asm.S plt.S mouse.S
ASOBJS = $(ASSRCS:.S=.o)
LIBCMD = libcomcom64.so
CMD = comcom64.exe
ELF = comcom64.elf
DBG = dosemu_$(CMD).dbg
REVISIONID := $(shell git describe --dirty=+)
C_OPT += -DREV_ID=\"$(REVISIONID)\"
srcdir ?= .
SRC = $(srcdir)

.PHONY: all clean install uninstall

all: $(CMD)

clean:
	$(RM) $(CMD) $(LIBCMD) $(ELF) *.dbg *.o version
	$(RM) *.asm plt.inc *.tmp thunk_calls.h thunk_asms.h

$(OBJS): $(HDRS)
$(LIBCMD): $(OBJS)
	$(HOST_LD) $^ $(LINK_OPT) -o $@

$(ELF): $(ASOBJS)
	$(DOS_LD) $^ $(DOSLDFLAGS) -o $@
	$(DOS_STRIP) $@

$(CMD): $(LIBCMD) $(ELF)
	./link.sh djlink objdump $(ELF) $(LIBCMD) $(DBG) $@ $(CMD)

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

TGMK = $(shell pkg-config --variable=makeinc thunk_gen)
ifeq ($(TGMK),)
ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error thunk_gen not installed)
endif
endif
TFLAGS = -a 4 -p 4
PDHDR = $(SRC)/asm.h
include $(TGMK)

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
