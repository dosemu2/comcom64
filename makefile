-include config.mak
srcdir ?= $(CURDIR)

RELVER = alpha3
PKG = comcom64-0.1$(RELVER)
TGZ = $(PKG).tar.gz

all: 64
both: static 32

install:
	$(MAKE) -C src install

install_32:
	$(MAKE) -C src/32 install

install_both: install install_32

uninstall:
	$(MAKE) -C src uninstall

uninstall_32:
	$(MAKE) -C src/32 uninstall

uninstall_both: uninstall uninstall_32

clean:
	$(MAKE) -C src clean
	$(MAKE) -C src/32 clean
	$(RM) -f $(TGZ) *.zip

distclean:
	git clean -fd || $(MAKE) clean

$(TGZ):
	git archive -o $(CURDIR)/$(TGZ) --prefix=$(PKG)/ HEAD
.PHONY: $(TGZ) 64 32 both djgpp install install_32 install_both uninstall_both uninstall uninstall_32

tar: $(TGZ)

rpm: comcom64.spec.rpkg
	git clean -fd
	rpkg local

deb:
	debuild -i -us -uc -b

64:
	$(MAKE) -C src

32:
	$(MAKE) -C src/32

djgpp:
	$(MAKE) -C djgpp

static:
	$(MAKE) -C src static

fetch:
	curl -O https://dosemu2.github.io/comcom64/files/comcom64.zip
	unzip -o comcom64.zip -d src
	curl -O https://dosemu2.github.io/comcom64/files/comcom32.zip
	unzip -o comcom32.zip -d 32
