#!/bin/sh
# compare_mem.sh: run comcom64's built-in MEM against the real FreeDOS
# MEM.EXE, in the same dosemu2 session, and print both outputs so the
# numbers can be compared by hand (or diffed by a future caller).
#
# This does NOT try to byte-for-byte diff the two outputs: they format
# things differently (KB vs raw bytes, extra "Reserved" category, etc.)
# and, since FDMEM.EXE is loaded as an external program while comcom64's
# MEM is built into the shell, the two tools observe slightly different
# conventional-memory states (FDMEM's own PSP/environment/program taking
# up space that our built-in command doesn't need). We run our own MEM
# first so it always sees the pristine, nothing-else-loaded state, then
# FDMEM second.
#
# What IS asserted automatically: EMS total/free and XMS free, which are
# independent of which shell command loaded them and should match
# exactly between the two tools' reports.
#
# Usage: ./tests/compare_mem.sh [path/to/comcom64.exe]
#   Defaults to src/comcom64.exe relative to the repo root, building it
#   first via "make" in src/ if it isn't there yet.
#
# Env vars:
#   CACHE_DIR       - where to cache the downloaded FreeDOS MEM.EXE
#                      (default: ~/.cache/comcom64-tests)
#   FREEDOS_MEM_URL - override the mem.zip URL (default: FreeDOS 1.4 repo)
#   EMS_SIZE_KB     - EMS pool size to configure dosemu with (default: 4096)

set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

CACHE_DIR=${CACHE_DIR:-"$HOME/.cache/comcom64-tests"}
FREEDOS_MEM_URL=${FREEDOS_MEM_URL:-"https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.4/base/mem.zip"}
EMS_SIZE_KB=${EMS_SIZE_KB:-4096}

COMCOM_EXE=${1:-"$REPO_ROOT/src/comcom64.exe"}

if [ ! -f "$COMCOM_EXE" ]; then
    echo "== $COMCOM_EXE not found, building it =="
    make -C "$REPO_ROOT/src" -j"$(nproc)"
fi

mkdir -p "$CACHE_DIR"
if [ ! -f "$CACHE_DIR/FDMEM.EXE" ]; then
    echo "== Fetching real FreeDOS MEM.EXE (cached under $CACHE_DIR) =="
    curl -sSL -o "$CACHE_DIR/mem.zip" "$FREEDOS_MEM_URL"
    rm -rf "$CACHE_DIR/mem_extracted"
    unzip -oq "$CACHE_DIR/mem.zip" -d "$CACHE_DIR/mem_extracted"
    cp "$CACHE_DIR/mem_extracted/BIN/MEM.EXE" "$CACHE_DIR/FDMEM.EXE"
fi

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

mkdir -p "$WORKDIR/localdir" "$WORKDIR/drive_c"
cp "$COMCOM_EXE" "$WORKDIR/drive_c/command.com"
cp "$CACHE_DIR/FDMEM.EXE" "$WORKDIR/drive_c/FDMEM.EXE"

cat > "$WORKDIR/drive_c/CMP.BAT" <<'EOF'
@echo off
echo ===COMCOM64-DEBUG===
mem /debug
echo ===COMCOM64-FREE===
mem /free
echo ===COMCOM64-CLASSIFY===
mem /classify
echo ===FDMEM-DEBUG===
fdmem /debug
echo ===FDMEM-FREE===
fdmem /free
echo ===FDMEM-CLASSIFY===
fdmem /classify
EOF

OUT="$WORKDIR/output.txt"
timeout 40 dosemu -td \
    --Flocal_dir "$WORKDIR/localdir" \
    --Fdrive_c "$WORKDIR/drive_c" \
    -e "$EMS_SIZE_KB" \
    -o "$WORKDIR/boot.log" \
    -E "cmp.bat" > "$OUT" 2>&1 || {
        echo "dosemu run failed; boot.log follows:" >&2
        cat "$WORKDIR/boot.log" >&2
        exit 1
    }

echo
echo "############################################################"
echo "# Full output (comcom64 first, then real FreeDOS MEM.EXE)"
echo "############################################################"
sed -n '/===COMCOM64-DEBUG===/,$p' "$OUT"
echo

# --- Automated checks: EMS and XMS-free are independent of which shell
# loaded the tool, so these should match exactly between both reports.
extract_bytes() {
    # $1 = file, $2 = label regex, prints the "(N bytes)" figure with
    # thousands separators stripped
    grep -E "$2" "$1" | head -1 | sed -E 's/.*\(([0-9,]+) bytes\).*/\1/' | tr -d ','
}

comcom_ems_free=$(extract_bytes "$OUT" '^Free Expanded \(EMS\)')
comcom_ems_total=$(extract_bytes "$OUT" '^Total Expanded \(EMS\)')

# The FDMEM section comes after the second "===FDMEM-DEBUG===" marker
sed -n '/===FDMEM-DEBUG===/,/===FDMEM-FREE===/p' "$OUT" > "$WORKDIR/fdmem_debug.txt"
fdmem_ems_free=$(extract_bytes "$WORKDIR/fdmem_debug.txt" '^Free Expanded \(EMS\)')
fdmem_ems_total=$(extract_bytes "$WORKDIR/fdmem_debug.txt" '^Total Expanded \(EMS\)')

status=0
echo "############################################################"
echo "# Automated checks (values that must match exactly)"
echo "############################################################"

check() {
    name=$1; a=$2; b=$3
    if [ "$a" = "$b" ] && [ -n "$a" ]; then
        echo "PASS: $name matches ($a bytes)"
    else
        echo "FAIL: $name differs: comcom64=$a freedos=$b"
        status=1
    fi
}

check "EMS total" "$comcom_ems_total" "$fdmem_ems_total"
check "EMS free"  "$comcom_ems_free"  "$fdmem_ems_free"

exit $status
