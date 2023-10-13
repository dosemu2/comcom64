RELVER = alpha3
PKG = comcom32-0.1$(RELVER)
TGZ = $(PKG).tar.gz

all:
	$(MAKE) -C src

install uninstall:
	$(MAKE) -C src $@

clean:
	$(MAKE) -C src clean
	$(RM) -f $(TGZ) *.zip

distclean:
	git clean -fd

$(TGZ):
	git archive -o $(CURDIR)/$(TGZ) --prefix=$(PKG)/ HEAD
.PHONY: $(TGZ)

tar: $(TGZ)

rpm: comcom32.spec.rpkg
	git clean -fd
	rpkg local

deb:
	debuild -i -us -uc -A

checkinstall:
	checkinstall --nodoc -y -D $(MAKE) -C src install PREFIX=/usr

fetch:
	curl -O https://dosemu2.github.io/comcom32/files/comcom32.zip
	unzip -o comcom32.zip -d src
