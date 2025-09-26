#!/bin/sh

sudo add-apt-repository ppa:jwt27/djgpp-toolchain
sudo add-apt-repository ppa:dosemu2/ppa

sudo apt update -q

sudo apt install -y djstub gcc-djgpp dosemu2
make deb
sudo dpkg -i ../comcom64*.deb
