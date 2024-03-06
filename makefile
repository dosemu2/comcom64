RELVER = alpha3
PKG = comcom64-0.1$(RELVER)
TGZ = $(PKG).tar.gz
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

all: 64
both: 64 32

install uninstall:
	$(MAKE) -C src $@

clean:
	$(MAKE) -C src clean
	$(MAKE) -C 32 clean
	$(RM) -f $(TGZ) *.zip

distclean:
	git clean -fd || $(MAKE) clean

$(TGZ):
	git archive -o $(CURDIR)/$(TGZ) --prefix=$(PKG)/ HEAD
.PHONY: $(TGZ) 64 32 both

tar: $(TGZ)

rpm: comcom64.spec.rpkg
	git clean -fd
	rpkg local

deb:
	debuild -i -us -uc -b

64:
	$(MAKE) -C src CROSS_PREFIX=$(CROSS_PREFIX)

32:
	$(MAKE) -C 32
