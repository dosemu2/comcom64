#!/bin/sh

sudo add-apt-repository -y --no-update ppa:stsp-0/thunk-gen
sudo add-apt-repository -y --no-update ppa:stsp-0/dj64
sudo apt update -q

sudo apt install -y \
  devscripts \
  equivs

mk-build-deps --install --root-cmd sudo

# Ensure the test-binaries link points to ~/cache
[ -h "test-binaries" ] || ln -s "${HOME}"/cache "test-binaries"

# Now grab parts of dosemu2 that we need
wget --no-verbose https://github.com/dosemu2/dosemu2/archive/refs/heads/devel.tar.gz
tar -xf devel.tar.gz --strip-components=1 dosemu2-devel/test
