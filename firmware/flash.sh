#!/usr/bin/env bash
set -euo pipefail

# Flash the current board's .hex to the connected programmer.
# Run from a board directory (board1-switching, board2-buttons, board3-main).
#
#   ./flash.sh            flash the production build (dist/default/...)
#   ./flash.sh debug      flash the debug build      (dist/debug/...)
#
# Override the programmer with PROG=<tool>, e.g. PROG=PK4 ./flash.sh
# IPECMD short names: RICE ICD3 ICD4 ICD5 PK3 PK4 PK5 SNAP PKOB PKOB4
#                     EDBG mEDBG nEDBG ICE4 PM3 J32
#
# ── Why the device is 18F57Q84 when the silicon is 18F27Q84 ───────────────────
# The DFP ships programming/debug scripts for pic18f57q84 only; there is no
# pic18f27q84 script directory, so selecting the real part fails with
# "No tool device file" and the tool never initialises. Selecting the 57Q84
# gets the tool up, and the two parts program identically here (the 27Q84 is
# the smaller-memory sibling, and our image fits).
#
# The consequence is a device-ID mismatch: the tool expects 0x9905 and reads
# 0x9903. MPLAB IPE asks whether to continue; IPECMD just aborts. Getting past
# it on the command line needs two local SDK edits, in BOTH pack locations
# (/Applications/microchip/mplabx/<ver>/packs and ~/.mchp_packs):
#
#   scripts/pic18f57q84/pic18f57q84pds.py   DEVICE_ID = 0x9905 -> 0x9903
#   edc/PIC18F57Q84.PIC                     DeviceIDSector edc:value 0x9905 -> 0x9903
#
# Both are root-owned, so they need sudo, and a pack or SDK update silently
# reverts them - at which point this script starts failing again with
# "Invalid Device ID". Keep .orig backups alongside.

IPECMD="/Applications/microchip/mplabx/v6.25/mplab_platform/mplab_ipe/bin/ipecmd.sh"
PROG="${PROG:-nEDBG}"
MCU="18F57Q84"
DFP="PIC18F-Q_DFP,1.27.449,Microchip"

CONF=default
case "${1:-}" in
    debug) CONF=debug ;;
    "")    ;;
    *)     echo "usage: $(basename "$0") [debug]" >&2; exit 2 ;;
esac

BOARD="$(basename "$(pwd)")"
HEX="dist/${CONF}/production/${BOARD}.production.hex"

if [ ! -f "$HEX" ]; then
    if [ "$CONF" = debug ]; then
        echo "no hex at $HEX — build first with ../build.sh debug" >&2
    else
        echo "no hex at $HEX — build first with ../build.sh" >&2
    fi
    exit 1
fi

# ipecmd writes log files into CWD; do it from a tmp dir to keep the tree clean.
LOGDIR="$(mktemp -d)"
trap 'rm -rf "$LOGDIR"' EXIT
HEX_ABS="$(cd "$(dirname "$HEX")" && pwd)/$(basename "$HEX")"

cd "$LOGDIR"
"$IPECMD" "-P${MCU}" "-TP${PROG}" "-F${HEX_ABS}" "-OWD${DFP}" -M -OL
