#!/bin/sh

sudo add-apt-repository -y --no-update ppa:dosemu2/ppa
sudo add-apt-repository -y --no-update ppa:jwt27/djgpp-toolchain
sudo add-apt-repository -y --no-update ppa:stsp-0/gcc-ia16
sudo apt update -q

sudo apt install -y ../comcom32*.deb ../comcom64*.deb dosemu2 fdpp

# Originally from dosemu2
sudo apt install -y \
  nasm \
  python3-cpuinfo \
  python3-pexpect \
  mtools \
  gcc-djgpp \
  djgpp-dev \
  qemu-system-common \
  gdb \
  valgrind \
  gcc-ia16-elf \
  libi86-ia16-elf
