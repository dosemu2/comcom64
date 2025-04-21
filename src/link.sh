#!/bin/sh

die() {
    echo "$1"
    exit 1
}

LNK=$1
shift
L=$1
shift
D=$1
shift
O=$1
shift

OD=`which objdump 2>/dev/null`
[ -n "$OD" ] || OD=`which llvm-objdump 2>/dev/null`
[ -n "$OD" ] || die "objdump not found"

FLG=$($OD -T $L | grep _shm_flags | sed -E 's/0*([^ ]+) .+/0x\1/')
CMD="$LNK -d $D $L -f $FLG -o $O $*"
echo $CMD
$CMD
