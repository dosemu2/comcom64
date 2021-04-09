#!/bin/sh

sudo add-apt-repository ppa:dosemu2/ppa
sudo add-apt-repository ppa:jwt27/djgpp-toolchain

sudo apt update -q

sudo apt install -y \
  acl \
  gcc-djgpp \
  devscripts \
  debhelper
