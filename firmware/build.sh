#!/usr/bin/env bash
set -euo pipefail

# Usage, from a board directory (board1-switching, board2-buttons, board3-main):
#
#   ../build.sh              production build  -> dist/default/production/
#   ../build.sh debug        debug build       -> dist/debug/production/
#   ../build.sh clean        remove both
#
# A debug build defines __DEBUG=1 and reserves the on-chip debug resources
# for a tool via -mdebugger. XC8 3.10 has no value for the Curiosity Nano's
# on-board PKOB nano, so it borrows a PICkit-class reservation by default;
# override when using a different probe:
#
#   DEBUGGER=snap ../build.sh debug
#
# Accepted by this compiler: none icd3 icd4 icd5 realice pickit3 pickit4
# pickit5 snap ice4.
#
# The two configurations build into separate build/ and dist/ trees, so a
# debug build never clobbers the production hex.

IMAGE="pic18f-xc8:3.10"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BOARD="$(basename "$(pwd)")"
DEBUGGER="${DEBUGGER:-pickit4}"

CONF=default
EXTRA_CFLAGS=""

case "${1:-}" in
    debug) CONF=debug; EXTRA_CFLAGS="-D__DEBUG=1 -mdebugger=${DEBUGGER}" ;;
    clean) CONF=clean ;;
    "")    ;;
    *)     echo "usage: $(basename "$0") [debug|clean]" >&2; exit 2 ;;
esac

# ── Build the Docker image (cached after first run) ───────────────────────────
docker build --platform linux/amd64 -t "${IMAGE}" "${SCRIPT_DIR}/toolchain"

# ── Clean ─────────────────────────────────────────────────────────────────────
if [ "${CONF}" = "clean" ]; then
    for c in default debug; do
        docker run --rm --platform linux/amd64 \
            -v "${SCRIPT_DIR}:/dist" \
            "${IMAGE}" \
            make -C "/dist/${BOARD}" clean CONF="${c}"
    done
    rm -rf build dist
    exit 0
fi

# ── Compile ───────────────────────────────────────────────────────────────────
# Mount the firmware root so sibling directories (e.g. libcomm) are accessible.
# The board to build is passed as an argument to make via -C.
#
# EXTRA_CFLAGS is a command-line variable, so make forwards it through
# MAKEFLAGS to the recursive libcomm build without it being named there.
docker run --rm --platform linux/amd64 \
    -v "${SCRIPT_DIR}:/dist" \
    "${IMAGE}" \
    make -C "/dist/${BOARD}" -r dist \
        CONF="${CONF}" \
        EXTRA_CFLAGS="${EXTRA_CFLAGS}" \
        IGNORE_LOCAL=TRUE \
        MP_CC=/opt/microchip/xc8/bin/xc8-cc \
        MP_CC_DIR=/opt/microchip/xc8/bin/ \
        MP_AS=/opt/microchip/xc8/bin/xc8-cc \
        MP_LD=/opt/microchip/xc8/bin/xc8-cc \
        MP_AR=/opt/microchip/xc8/bin/xc8-ar \
        DFP_DIR=/opt/packs/Microchip/PIC18F-Q_DFP/1.27.449
