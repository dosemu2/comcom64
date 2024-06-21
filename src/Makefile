PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom64
CFLAGS = -Wall -Os -Wmissing-declarations -Wwrite-strings \
  -ggdb3 -Wunused -Wmissing-prototypes
SOURCES = command.c cmdbuf.c mouse.c env.c psp.c umb.c ae0x.c compl.c \
  thunks_a.c thunks_c.c
HEADERS = ae0x.h cmdbuf.h compl.h psp.h command.h env.h mouse.h umb.h \
  glob_asm.h asm.h
PDHDR = asm.h
OBJECTS = $(SOURCES:.c=.o)
AS_SOURCES = int23.S int0.S asm.S ms.S
AS_OBJECTS = $(AS_SOURCES:.S=.o)
CMD = comcom64.exe
DBG = dosemu_$(CMD).dbg
REVISIONID := $(shell git describe --dirty=+)
CFLAGS += -DREV_ID=\"$(REVISIONID)$(STEXT)\"

.PHONY: all clean install uninstall

all: $(CMD)

static:
	$(MAKE) DJ64STATIC=1

clean: clean_dj64
	$(RM) $(CMD)

$(OBJECTS): $(HEADERS)
$(AS_OBJECTS): asm.h asm.inc

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

# hook in dj64 - make sure to not do that before defining `all:` target
DJMK = $(shell pkg-config --variable=makeinc dj64)
ifeq ($(DJMK),)
ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error dj64 not installed)
endif
clean_dj64:
else
include $(DJMK)
endif

ifeq ($(DJ64STATIC),1)
STEXT = \(static_link\)
endif

$(CMD): $(DJ64_XOBJS)
	./link.sh djlink $(DJ64_XLIB) $(DBG) $@ $(CMD) $(DJ64_XLDFLAGS)

info: $(CMD)
	djstubify -i $<
