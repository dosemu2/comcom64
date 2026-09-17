#!/bin/sh

# Ensure the test-binaries link points to ~/cache
[ -h "test-binaries" ] || ln -s "${HOME}"/cache "test-binaries"

# Now grab parts of dosemu2 that we need
wget --no-verbose https://github.com/dosemu2/dosemu2/archive/refs/heads/devel.tar.gz
tar -xf devel.tar.gz --strip-components=1 dosemu2-devel/test

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
