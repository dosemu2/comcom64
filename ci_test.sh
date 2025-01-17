#!/bin/sh

set -e

sudo dpkg -i ../comcom64*.deb

. ./ci_test_prereq.sh

if ! dosemu -td -o boot.log -E ver ; then
  {
    echo "================== boot.log ==================="
    cat boot.log
    echo "==============================================="
  } >&2
  exit 1
fi

make 32 -j 9
# make sure 32bit version also built
ls -l 32/comcom32.exe
