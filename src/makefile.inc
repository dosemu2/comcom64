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
  djansi.c thunks_a.c thunks_c.c
HEADERS = $(addprefix $(srcdir)/,ae0x.h cmdbuf.h compl.h psp.h command.h env.h mouse.h umb.h \
  glob_asm.h asm.h)
PDHDR = $(srcdir)/asm.h
GLOB_ASM = $(srcdir)/glob_asm.h
OBJECTS = $(SOURCES:.c=.o)
AS_SOURCES = int21.S int23.S int0.S int75.S asm.S ms.S
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

# static re-executes make, so we include that later
ifeq ($(filter static install uninstall,$(MAKECMDGOALS)),)
# hook in dj64 - make sure to not do that before defining `all:` target
DJMK = $(shell pkg-config --variable=makeinc $(TOOLC))
ifeq ($(wildcard $(DJMK)),)
ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error $(TOOLC)-dev not installed)
endif
clean_dj64:
clean_dj32:
else
include $(DJMK)
endif
endif

ifeq ($(TOOLC),dj32)
ifneq ($(CROSS_PREFIX),)
# for djlink on arm
export READELF = $(CROSS_PREFIX)readelf
export OBJCOPY = $(CROSS_PREFIX)objcopy
export STRIP = $(CROSS_PREFIX)strip
endif
else
ifneq ($(NATIVE_PREFIX),)
# for djlink on sunos
export READELF = $(NATIVE_PREFIX)readelf
export OBJCOPY = $(NATIVE_PREFIX)objcopy
export STRIP = $(NATIVE_PREFIX)strip
endif
endif

ifneq ($(LINK),)
$(RCMD): $(DJ64_XLIB)
	$(srcdir)/link.sh $(LINK) $^ $(DBG) "$(DJ64_LINKARGS)" $@ $(DJ64_XLDFLAGS)
	$(DJSTRIP) $@

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
