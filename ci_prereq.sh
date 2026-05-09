#!/bin/sh

sudo apt update
sudo apt install -y \
  devscripts \
  equivs

sudo add-apt-repository ppa:stsp-0/thunk-gen
sudo add-apt-repository ppa:stsp-0/dj64
mk-build-deps --install --root-cmd sudo
# djstub is optional, but it avoids lintian errors on make deb
sudo apt install -y djstub
