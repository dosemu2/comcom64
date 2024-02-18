RELVER = alpha3
PKG = comcom64-0.1$(RELVER)
TGZ = $(PKG).tar.gz

all:
	$(MAKE) -C src

install uninstall:
	$(MAKE) -C src $@

clean:
	$(MAKE) -C src clean
	$(RM) -f $(TGZ) *.zip

distclean:
	git clean -fd || $(MAKE) clean

$(TGZ):
	git archive -o $(CURDIR)/$(TGZ) --prefix=$(PKG)/ HEAD
.PHONY: $(TGZ)

tar: $(TGZ)

rpm: comcom64.spec.rpkg
	git clean -fd
	rpkg local

deb:
	debuild -i -us -uc -b

checkinstall:
	checkinstall --nodoc -y -D $(MAKE) -C src install PREFIX=/usr

fetch:
	curl -O https://dosemu2.github.io/comcom64/files/comcom64.zip
	unzip -o comcom64.zip -d src
