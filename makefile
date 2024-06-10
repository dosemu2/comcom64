RELVER = alpha3
PKG = comcom64-0.1$(RELVER)
TGZ = $(PKG).tar.gz

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
	$(MAKE) -C src

32:
	$(MAKE) -C 32

static:
	$(MAKE) -C src DJ64STATIC=1
