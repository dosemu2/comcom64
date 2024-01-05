#!/bin/sh

set -e

sudo dpkg -i ../comcom32*.deb
sudo apt-get install -qq -f dosemu2 fdpp
dosemu -td -E ver
