#!/bin/sh

sudo apt update
sudo apt install -y \
  devscripts \
  equivs

sudo add-apt-repository -y ppa:stsp-0/thunk-gen || (sleep 5 && sudo add-apt-repository -y ppa:stsp-0/thunk-gen)
sudo add-apt-repository -y ppa:stsp-0/dj64 || (sleep 5 && sudo add-apt-repository -y ppa:stsp-0/dj64)
mk-build-deps --install --root-cmd sudo
