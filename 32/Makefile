TOP = ..
SRCDIR = $(TOP)/src
DOS_CC ?= i586-pc-msdosdjgpp-gcc
DOS_LD ?= i586-pc-msdosdjgpp-gcc
DOS_AS ?= i586-pc-msdosdjgpp-as
DOS_STRIP ?= i586-pc-msdosdjgpp-strip
PREFIX ?= /usr/local
DATADIR ?= $(PREFIX)/share/comcom32
C_OPT = -Wall -O2 -Wmissing-declarations -Wwrite-strings -I $(SRCDIR) -I .
# avoid CMOVxx instructions
C_OPT += -march=i386
LINK_OPT =
SRCS = $(SRCDIR)/command.c $(SRCDIR)/cmdbuf.c $(SRCDIR)/ms.c \
  $(SRCDIR)/version.c memmem.c fmemcpy.c
ASSRCS = $(SRCDIR)/asm.S $(SRCDIR)/int23.S $(SRCDIR)/int0.S $(SRCDIR)/mouse.S
OBJS = $(notdir $(SRCS:.c=.o)) $(notdir $(ASSRCS:.S=.o))
CMD = comcom32.exe
REVISIONID := $(shell git describe --dirty=+)

.PHONY: all clean install uninstall

ifneq ($(shell $(DOS_CC) --version 2>/dev/null),)
all: $(CMD)
else
all:
endif

clean:
	$(RM) $(CMD) *.o version

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
%.o : $(SRCDIR)/%.c
	$(DOS_CC) $(C_OPT) -c $< -o $@

%.o : %.c
	$(DOS_CC) $(C_OPT) -c $< -o $@

%.o : $(SRCDIR)/%.S
	$(DOS_CC) $(C_OPT) -c $< -o $@

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
