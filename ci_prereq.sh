#!/bin/sh

sudo add-apt-repository -y --no-update ppa:stsp-0/thunk-gen
sudo add-apt-repository -y --no-update ppa:stsp-0/dj64
sudo apt update -q

sudo apt install -y \
  devscripts \
  equivs

mk-build-deps --install --root-cmd sudo
