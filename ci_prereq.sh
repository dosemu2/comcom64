#!/bin/sh

sudo add-apt-repository ppa:dosemu2/ppa
sudo add-apt-repository ppa:stsp-0/thunk-gen
sudo add-apt-repository ppa:stsp-0/dj64

sudo apt update -q

sudo apt install -y \
  acl \
  dj64-dev \
  djstub \
  thunk-gen \
  pkgconf \
  devscripts \
  debhelper
