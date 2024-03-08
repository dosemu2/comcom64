#!/bin/sh

sudo apt-get update
sudo apt install -y \
  devscripts \
  equivs

sudo add-apt-repository ppa:stsp-0/thunk-gen
sudo add-apt-repository ppa:stsp-0/dj64
mk-build-deps --install --root-cmd sudo
