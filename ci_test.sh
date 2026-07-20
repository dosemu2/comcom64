#!/bin/sh

set -e

export TEST_DOSEMU=/usr/bin/dosemu
export TEST_CMDDIR=/usr/share/dosemu/dosemu2-cmds-0.3
export NO_FAILFAST=1

cat >&2 << EOF3
=====================================================
=              Tests run on Comcom32                =
=====================================================
EOF3
env COPY_COMMAND_COM=/usr/share/comcom32/comcom32.exe test/test_comcom.py TestCase32

cat >&2 << EOF4
=====================================================
=              Tests run on Comcom64                =
=====================================================
EOF4
env COPY_COMMAND_COM=/usr/share/comcom64/comcom64.exe test/test_comcom.py TestCase64

make both -j 9
ls -l src/32/comcom32.exe

# Return non-zero if any logfiles were generated
for i in test_*.*.*.log ; do
  test -f $i || exit 0
done

exit 1
