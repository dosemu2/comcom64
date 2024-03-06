#!/bin/sh

set -e

sudo dpkg -i ../comcom64*.deb

. ./ci_test_prereq.sh

dosemu -td -E ver

make 32 -j 9
# make sure 32bit version also built
ls -l 32/comcom32.exe
