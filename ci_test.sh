#!/bin/sh

set -e

sudo dpkg -i ../comcom32*amd64.deb
sudo apt-get install -qq -f dosemu2 fdpp
dosemu -td -E ver
