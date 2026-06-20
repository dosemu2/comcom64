#!/bin/sh

set -e

make deb
sudo dpkg -i ../comcom64*.deb
sudo dpkg -i ../comcom32*.deb
