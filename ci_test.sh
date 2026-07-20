#!/usr/bin/bash

set +e

export TEST_DOSEMU=/usr/bin/dosemu
export TEST_CMDDIR=/usr/share/dosemu/dosemu2-cmds-0.3
export NO_FAILFAST=1

VERSION=32
cat >&2 << EOF3
=====================================================
=              Tests run on Comcom${VERSION}                =
=====================================================
EOF3
env NO_FAILFAST=1 COPY_COMMAND_COM=/usr/share/comcom${VERSION}/comcom${VERSION}.exe test/test_comcom.py TestCase${VERSION}

VERSION=64
cat >&2 << EOF4
=====================================================
=              Tests run on Comcom${VERSION}                =
=====================================================
EOF4
env NO_FAILFAST=1 COPY_COMMAND_COM=/usr/share/comcom${VERSION}/comcom${VERSION}.exe test/test_comcom.py TestCase${VERSION}

# Return non-zero if any logfiles were generated
for i in test_*.*.*.log ; do
  test -f $i || exit 0
done

exit 1
