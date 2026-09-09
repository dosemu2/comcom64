#!/bin/sh

sudo add-apt-repository -y --no-update ppa:dosemu2/ppa
sudo apt update -q

sudo apt install -f -y ../comcom32*.deb ../comcom64*.deb dosemu2 fdpp  # dj64-dev-static

# Just to make sure that the PPAs got installed
ls -l /usr/share/comcom??/comcom??.exe

# Ensure the test-binaries link points to ~/cache
[ -h "test-binaries" ] || ln -s "${HOME}"/cache "test-binaries"

# Now grab parts of dosemu2 that we need
wget https://github.com/dosemu2/dosemu2/archive/refs/heads/devel.tar.gz
tar -xvf devel.tar.gz --strip-components=1 dosemu2-devel/test

# This is from dosemu2
sudo add-apt-repository -y --no-update ppa:jwt27/djgpp-toolchain
sudo add-apt-repository -y --no-update ppa:stsp-0/gcc-ia16
sudo apt update -q

sudo apt install -y \
  acl \
  cpu-checker \
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
  libi86-ia16-elf \
  libi86-testsuite-ia16-elf \
  gcc-multilib \
  dos2unix \
  bridge-utils \
  libvirt-daemon \
  libvirt-daemon-system

sudo apt install -y \
  dj64-dbgsym \
  djdev64-dbgsym

# Install the FAT mount helper
sudo cp test/dosemu_fat_mount.sh /bin/.
sudo chown root:root /bin/dosemu_fat_mount.sh
sudo chmod 755 /bin/dosemu_fat_mount.sh

# Install the TAP helper
sudo cp test/dosemu_tap_interface.sh /bin/.
sudo chown root:root /bin/dosemu_tap_interface.sh
sudo chmod 755 /bin/dosemu_tap_interface.sh
