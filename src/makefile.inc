-include config.mak
VERSION = 0.4
srcdir ?= $(CURDIR)
vpath %.c $(srcdir)
vpath %.S $(srcdir)
# on Termux PREFIX is used
ifneq ($(PREFIX),)
prefix := $(PREFIX)
else
prefix ?= /usr/local
endif
DATADIR ?= $(prefix)/share/$(PRJ)
DJCFLAGS = -Wall -O2 -Wmissing-declarations -Wwrite-strings \
  -ggdb3 -Wunused -Wmissing-prototypes
# current dir must be searched before srcdir to avoid picking wrong gen files
DJCPPFLAGS += -I. -I$(srcdir) -DCOMCOM_VERSION=\"$(VERSION)\"
DJASFLAGS += -I. -I$(srcdir)
DJASCPPFLAGS += -I. -I$(srcdir)
SOURCES = command.c cmdbuf.c mouse.c env.c psp.c umb.c ae0x.c compl.c clip.c \
  thunks_a.c thunks_c.c
HEADERS = $(addprefix $(srcdir)/,ae0x.h cmdbuf.h compl.h psp.h command.h env.h mouse.h umb.h \
  glob_asm.h asm.h)
PDHDR = $(srcdir)/asm.h
GLOB_ASM = $(srcdir)/glob_asm.h
OBJECTS = $(SOURCES:.c=.o)
AS_SOURCES = int23.S int0.S int75.S asm.S ms.S
AS_OBJECTS = $(AS_SOURCES:.S=.o)
RCMD = $(PRJ).exe
CMD = $(if $(LINK),$(RCMD),)
DBG = $(if $(LINK),$(CMD).dbg,)
REVISIONID := $(shell git describe --dirty=+)
ifeq ($(REVISIONID),)
REVISIONID := Non-git_build
endif
DJCFLAGS += -DREV_ID=\"$(REVISIONID)\"
DJ64COMPACT_VA := 1

.PHONY: all _all clean install uninstall

# all: must be defined before including dj64.mk, and with _all: we delay the
# expansion of vars by defining it _after_ inclusion of dj64.mk
all: _all

static:
	$(MAKE) DJ64STATIC=1

$(OBJECTS): $(HEADERS)
$(AS_OBJECTS): $(PDHDR) $(srcdir)/asm.inc

uninstall:
	rm -rf $(DESTDIR)$(DATADIR)

# hook in dj64 - make sure to not do that before defining `all:` target
export PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):$(prefix)/share/pkgconfig:$(prefix)/lib/pkgconfig
DJMK = $(shell pkg-config --variable=makeinc $(TOOLC))
ifeq ($(wildcard $(DJMK)),)
ifeq ($(filter install uninstall clean,$(MAKECMDGOALS)),)
$(error dj64-dev not installed)
endif
clean_dj64:
clean_dj32:
else
include $(DJMK)
endif

ifeq ($(DJ64STATIC),1)
DJCFLAGS += -DSTATIC_LINK
endif

ifneq ($(LINK),)
$(RCMD): $(DJ64_XLIB)
	$(srcdir)/link.sh $(LINK) $^ $(DBG) $@ $(DJ64_XLDFLAGS)
	$(STRIP) $@

info: $(RCMD)
	djstubify -i $<
endif

clean: clean_$(TOOLC)
ifneq ($(CMD),)
	$(RM) $(CMD)
endif

ifeq (,$(wildcard $(RCMD))$(wildcard $(DJ64_XLIB)))
install:
	@echo "Build it first or run \"make fetch\"" && false
else
ifeq ($(wildcard $(RCMD)),)
LDR = $(shell pkg-config --variable=elfload dj64)
ifeq ($(wildcard $(LDR)),)
$(error elfload.com not found)
endif
endif
install:
endif
	mkdir -p $(DESTDIR)$(DATADIR)
ifneq ($(wildcard $(RCMD)),)
	$(RM) $(DESTDIR)$(DATADIR)/command.elf
	install -m 0644 $(RCMD) $(DESTDIR)$(DATADIR)
	ln -sf $(RCMD) $(DESTDIR)$(DATADIR)/command.com
else
	$(RM) $(DESTDIR)$(DATADIR)/$(PRJ).exe
	install -m 0644 $(DJ64_XLIB) $(DESTDIR)$(DATADIR)/command.elf
	install -m 0644 $(LDR) $(DESTDIR)$(DATADIR)/command.com
endif

_all: $(DJ64_XLIB) $(CMD)
