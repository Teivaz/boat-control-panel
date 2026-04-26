#!/usr/bin/env bash
set -euo pipefail

# Flash the current board's production .hex to a connected programmer.
# Run from a board directory (board1-switching, board2-buttons, board3-main).
# Override the programmer with PROG=<tool>, e.g. PROG=PK5 ./flash.sh
#   PKOB (default — Curiosity Nano on-board), PK4, PK5, ICD3, ICD4, ICD5, SNAP

IPECMD="/Applications/Microchip/mplabx/v6.25/mplab_platform/mplab_ipe/bin/ipecmd.sh"
PROG="${PROG:-PKOB}"
MCU="18F27Q84"

BOARD="$(basename "$(pwd)")"
HEX="dist/default/production/${BOARD}.production.hex"

if [ ! -f "$HEX" ]; then
    echo "no hex at $HEX — build first with ../build.sh" >&2
    exit 1
fi

# ipecmd writes log files into CWD; do it from a tmp dir to keep the tree clean.
LOGDIR="$(mktemp -d)"
trap 'rm -rf "$LOGDIR"' EXIT
HEX_ABS="$(cd "$(dirname "$HEX")" && pwd)/$(basename "$HEX")"

DFP="/Applications/Microchip/mplabx/v6.25/packs/Microchip/PIC18F-Q_DFP/1.27.449"

cd "$LOGDIR"
# "$IPECMD" "-P${MCU}" "-TP${PROG}" "-F${HEX_ABS}" "-OK${DFP}" -M -OL
"$IPECMD" "-P${MCU}" "-TP${PROG}" "-F${HEX_ABS}" -M -OL
