#!/bin/sh
# compare_mem.sh: run comcom64's built-in MEM against the real FreeDOS
# MEM.EXE, in the same dosemu2 session, and print both outputs so the
# numbers can be compared by hand (or diffed by a future caller).
#
# Both dosemu2 and FreeDOS come from the dosemu2 PPA: install dosemu2
# and install-freedos from ppa:dosemu2/ppa (see ci_test_prereq.sh), and
# this script then runs dosemu2's own "insfdusr" to install the FreeDOS
# userspace - which is where MEM.EXE comes from - instead of downloading
# and unpacking mem.zip by hand.
#
# The FreeDOS installation lives in a private HOME under $CACHE_DIR, so
# it is done once and reused, and the caller's own ~/.dosemu is left
# alone.
#
# The same comparison is done by dosemu2's own test suite, in
# test/func_comcom_internal.py, which CI runs - this script is the
# stand-alone version of it, for looking at the two reports side by side.
#
# This does NOT try to byte-for-byte diff the two outputs: they format
# the detail listings differently and, since MEM.EXE is loaded as an external program
# while comcom64's MEM is built into the shell, the two tools observe
# slightly different conventional-memory states (MEM.EXE's own PSP,
# environment and program taking up space that our built-in command
# doesn't need). We run our own MEM first so it always sees the
# pristine, nothing-else-loaded state, then MEM.EXE second.
#
# What IS asserted automatically: the figures that are independent of
# which tool loaded them - conventional and upper totals, the reserved
# region, XMS total/used/free, EMS total/free, free upper memory and the
# largest free upper block. Conventional used/free differ by about a KB,
# which is MEM.EXE's own PSP and environment - our MEM is built into the
# shell and needs neither - so they are not asserted here.
#
# Usage: ./tests/compare_mem.sh [path/to/comcom64-build]
#   The argument is the comcom64 to test: either a directory holding
#   command.com (as "make install" produces), or a comcom64.exe file.
#   It defaults to the build in src/, built via "make" if not there yet.
#   Without an argument and without a build, the comcom64 that dosemu2
#   is configured with is used as-is.
#
# Env vars:
#   CACHE_DIR   - where to keep the FreeDOS installation used for the
#                 comparison (default: ~/.cache/comcom64-tests)
#   EMS_SIZE_KB - EMS pool size to configure dosemu with (default: 4096)

set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

CACHE_DIR=${CACHE_DIR:-"$HOME/.cache/comcom64-tests"}
EMS_SIZE_KB=${EMS_SIZE_KB:-4096}

if ! command -v dosemu >/dev/null; then
    echo "dosemu2 not found - install it from ppa:dosemu2/ppa:" >&2
    echo "  sudo add-apt-repository ppa:dosemu2/ppa" >&2
    echo "  sudo apt install dosemu2 install-freedos" >&2
    exit 1
fi

# The comcom64 under test. dosemu2 picks the shell up from
# $DOSEMU2_COMCOM_DIR, so a build can be tested without installing it.
COMCOM=${1:-}
if [ -z "$COMCOM" ] && [ ! -f "$REPO_ROOT/src/comcom64.exe" ] &&
        [ ! -f "$REPO_ROOT/src/libtmp.so" ]; then
    echo "== no build in src/, building it =="
    make -C "$REPO_ROOT/src" -j"$(nproc)" || make -C "$REPO_ROOT" static
fi
if [ -z "$COMCOM" ]; then
    if [ -f "$REPO_ROOT/src/comcom64.exe" ]; then
        COMCOM="$REPO_ROOT/src/comcom64.exe"
    else
        COMCOM="$REPO_ROOT/src/libtmp.so"
    fi
fi

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

COMCOM_DIR="$WORKDIR/comcom"
mkdir -p "$COMCOM_DIR"
if [ -d "$COMCOM" ]; then
    cp -a "$COMCOM"/. "$COMCOM_DIR/"
elif [ "${COMCOM%.so}" != "$COMCOM" ]; then
    # dj64 static build: the loader runs the ELF next to it
    ELFLOAD=$(pkg-config --variable=elfload dj64)
    cp "$COMCOM" "$COMCOM_DIR/command.elf"
    cp "$ELFLOAD" "$COMCOM_DIR/command.com"
else
    cp "$COMCOM" "$COMCOM_DIR/command.com"
fi
echo "== testing comcom64 from $COMCOM =="
export DOSEMU2_COMCOM_DIR="$COMCOM_DIR"

# Private HOME: insfdusr installs FreeDOS into $HOME/.dosemu/drive_c.
TEST_HOME="$CACHE_DIR/home"
mkdir -p "$TEST_HOME"
export HOME="$TEST_HOME"

if [ ! -f "$TEST_HOME/.dosemu/drive_c/bin/mem.exe" ]; then
    echo "== installing the FreeDOS userspace with insfdusr =="
    timeout 600 dosemu -td -o "$WORKDIR/insfdusr.log" -E insfdusr \
        > "$WORKDIR/insfdusr.out" 2>&1 || {
            echo "insfdusr failed:" >&2
            cat "$WORKDIR/insfdusr.out" >&2
            exit 1
        }
    if [ ! -f "$TEST_HOME/.dosemu/drive_c/bin/mem.exe" ]; then
        echo "insfdusr did not install MEM.EXE - is install-freedos installed?" >&2
        exit 1
    fi
fi

cat > "$TEST_HOME/.dosemu/drive_c/cmpmem.bat" <<'EOF'
@echo off
echo ===COMCOM64-DEFAULT===
mem
echo ===COMCOM64-DEBUG===
mem /debug
echo ===COMCOM64-FREE===
mem /free
echo ===COMCOM64-CLASSIFY===
mem /classify
echo ===FDMEM-DEFAULT===
c:\bin\mem.exe
echo ===FDMEM-FREE===
c:\bin\mem.exe /free
echo ===FDMEM-CLASSIFY===
c:\bin\mem.exe /classify
EOF

OUT="$WORKDIR/output.txt"
timeout 120 dosemu -td \
    -e "$EMS_SIZE_KB" \
    -o "$WORKDIR/boot.log" \
    -E "cmpmem.bat" > "$OUT" 2>&1 || {
        echo "dosemu run failed; boot.log follows:" >&2
        cat "$WORKDIR/boot.log" >&2
        exit 1
    }

echo
echo "############################################################"
echo "# Full output (comcom64 first, then real FreeDOS MEM.EXE)"
echo "############################################################"
sed -n '/===COMCOM64-DEFAULT===/,$p' "$OUT"
echo

sed -n '/===COMCOM64-DEFAULT===/,/===COMCOM64-DEBUG===/p' "$OUT" > "$WORKDIR/comcom.txt"
sed -n '/===FDMEM-DEFAULT===/,/===FDMEM-FREE===/p' "$OUT" > "$WORKDIR/fdmem.txt"

# Both tools report the summary in KB now, so the figures can be compared
# directly. FreeDOS MEM prints "637K", comcom64 prints the same - strip
# the separators and the K suffix and compare the numbers.
field() {
    # $1 = file, $2 = row regex, $3 = column (1=total, 2=used, 3=free)
    grep -E "$2" "$1" | head -1 | sed -E 's/^.{16}//' | tr -d '\r' |
        awk -v col="$3" '{print $col}' | tr -d ',K'
}

status=0
echo "############################################################"
echo "# Automated checks (values that must match exactly)"
echo "############################################################"

check() {
    name=$1; a=$2; b=$3
    if [ "$a" = "$b" ] && [ -n "$a" ]; then
        echo "PASS: $name matches (${a}K)"
    else
        echo "FAIL: $name differs: comcom64=${a}K freedos=${b}K"
        status=1
    fi
}

check_row() {
    # $1 = label, $2 = row regex, $3 = column
    check "$1" "$(field "$WORKDIR/comcom.txt" "$2" "$3")" \
               "$(field "$WORKDIR/fdmem.txt" "$2" "$3")"
}

check_row "conventional total" '^Conventional' 1
check_row "upper total" '^Upper' 1
check_row "upper used" '^Upper' 2
check_row "upper free" '^Upper' 3
check_row "reserved total" '^Reserved' 1
check_row "reserved used"  '^Reserved' 2
check_row "XMS total" '^Extended \(XMS\)' 1
check_row "XMS used"  '^Extended \(XMS\)' 2
check_row "XMS free"  '^Extended \(XMS\)' 3
check_row "total under 1 MB" '^Total under 1 MB' 1

tail_field() {
    # $1 = file, $2 = label regex: the KB figure of a "label   NNNK" line.
    # FreeDOS MEM appends the byte count in brackets, so pick the first
    # number that carries the K suffix rather than the last field.
    grep -E "$2" "$1" | head -1 | tr -d '\r' |
        grep -oE '[0-9][0-9,]*K' | head -1 | tr -d ',K'
}

check "EMS total" "$(tail_field "$WORKDIR/comcom.txt" '^Total Expanded \(EMS\)')" \
                  "$(tail_field "$WORKDIR/fdmem.txt" '^Total Expanded \(EMS\)')"
check "EMS free"  "$(tail_field "$WORKDIR/comcom.txt" '^Free Expanded \(EMS\)')" \
                  "$(tail_field "$WORKDIR/fdmem.txt" '^Free Expanded \(EMS\)')"
# MEM.EXE calls this row "Largest free upper memory block", ours keeps
# the MS-DOS wording that the dosemu2 test suite looks for.
check "largest upper memory block" \
      "$(tail_field "$WORKDIR/comcom.txt" '^Largest available upper')" \
      "$(tail_field "$WORKDIR/fdmem.txt" '^Largest free upper')"

exit $status
