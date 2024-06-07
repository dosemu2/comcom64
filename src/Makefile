GAS = $(CROSS_PREFIX)as
CROSS_PREFIX := i686-linux-gnu-
ifeq ($(shell $(GAS) --version 2>/dev/null),)
CROSS_PREFIX := x86_64-linux-gnu-
endif
ifeq ($(shell $(GAS) --version 2>/dev/null),)
ifeq ($(shell uname -m),x86_64)
CROSS_PREFIX :=
else
$(error cross-binutils not installed)
endif
endif
XAS = gcc -E -x assembler-with-cpp $< | $(GAS) --32

LD = gcc
XLD = $(CROSS_PREFIX)ld
XSTRIP = $(CROSS_PREFIX)strip --strip-debug
PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom64
CFLAGS = -Wall -Og -Wmissing-declarations -Wwrite-strings \
  -ggdb3 -Wunused -Wmissing-prototypes
LDFLAGS = $(shell pkg-config --libs dj64) \
  -Wl,-rpath=/usr/local/i386-pc-dj64/lib64 \
  -Wl,-rpath=/usr/i386-pc-dj64/lib64
XLDFLAGS = $(shell pkg-config --static --libs dj64static) -melf_i386 -static
CPPFLAGS = -I. $(shell pkg-config --variable=cppflags dj64)
SOURCES = command.c cmdbuf.c ms.c env.c psp.c umb.c ae0x.c compl.c \
  thunks_a.c thunks_c.c
HEADERS = ae0x.h cmdbuf.h compl.h psp.h command.h env.h ms.h umb.h \
  glob_asm.h asm.h
OBJECTS = $(SOURCES:.c=.o)
AS_SOURCES = int23.S int0.S asm.S mouse.S
AS_OBJECTS = $(AS_SOURCES:.S=.o)
LIB = libcomcom64.so
CMD = comcom64.exe
ELF = comcom64.elf
DBG = dosemu_$(CMD).dbg
REVISIONID := $(shell git describe --dirty=+)
CFLAGS += -DREV_ID=\"$(REVISIONID)\"
srcdir ?= .
SRC = $(srcdir)

.PHONY: all clean install uninstall

all: $(CMD)

clean:
	$(RM) $(CMD) $(LIB) $(ELF) *.dbg *.o version
	$(RM) *.asm plt.inc *.tmp thunk_calls.h thunk_asms.h

$(OBJECTS): $(HEADERS)
$(LIB): $(OBJECTS)
	$(LD) $^ $(LDFLAGS) -o $@

$(ELF): $(AS_OBJECTS) plt.o
	$(XLD) $^ $(XLDFLAGS) -o $@
	$(XSTRIP) $@

$(CMD): $(LIB) $(ELF)
	./link.sh djlink objdump $(ELF) $(LIB) $(DBG) $@ $(CMD)

#.INTERMEDIATE: $(LIB)

# Common rules
%.o: %.c
	dj64-gcc $(CFLAGS) -I. -o $@ -c $<

%.o: %.S
	$(XAS) -o $@

%.o: %.asm
	$(XAS) -o $@

plt.asm: $(srcdir)/plt.s plt.inc $(SRC)/glob_asm.h
	$(CPP) $(CPPFLAGS) -x assembler-with-cpp -P $< >$@ || ($(RM) $@ ; false)

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
