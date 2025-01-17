#!/bin/sh

set -e

if ! dosemu -td -o boot.log -E ver ; then
  {
    echo "================== boot.log ==================="
    cat boot.log
    echo "==============================================="
  } >&2
  exit 1
fi

# make sure 32bit version also built
ls -l 32/comcom32.exe
