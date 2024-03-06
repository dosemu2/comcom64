RELVER = alpha3
PKG = comcom64-0.1$(RELVER)
TGZ = $(PKG).tar.gz

all:
	$(MAKE) -C src

both:
	$(MAKE) -C src
	$(MAKE) -C 32

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
.PHONY: $(TGZ) 32 both

tar: $(TGZ)

rpm: comcom64.spec.rpkg
	git clean -fd
	rpkg local

deb:
	debuild -i -us -uc -b

32:
	$(MAKE) -C 32
