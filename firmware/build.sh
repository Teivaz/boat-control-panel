#!/usr/bin/env bash
set -euo pipefail

IMAGE="pic18f-xc8:3.10"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BOARD="$(basename "$(pwd)")"

# ── Build the Docker image (cached after first run) ───────────────────────────
docker build --platform linux/amd64 -t "${IMAGE}" "${SCRIPT_DIR}/toolchain"

# ── Clean ─────────────────────────────────────────────────────────────────────
if [ "${1:-}" = "clean" ]; then
    docker run --rm --platform linux/amd64 \
        -v "${SCRIPT_DIR}:/dist" \
        "${IMAGE}" \
        make -C "/dist/${BOARD}" clean CONF=default
    rm -rf build dist
    exit 0
fi

# ── Compile ───────────────────────────────────────────────────────────────────
# Mount the firmware root so sibling directories (e.g. libcomm) are accessible.
# The board to build is passed as an argument to make via -C.
docker run --rm --platform linux/amd64 \
    -v "${SCRIPT_DIR}:/dist" \
    "${IMAGE}" \
    make -C "/dist/${BOARD}" -r dist \
        CONF=default \
        IGNORE_LOCAL=TRUE \
        MP_CC=/opt/microchip/xc8/bin/xc8-cc \
        MP_CC_DIR=/opt/microchip/xc8/bin/ \
        MP_AS=/opt/microchip/xc8/bin/xc8-cc \
        MP_LD=/opt/microchip/xc8/bin/xc8-cc \
        MP_AR=/opt/microchip/xc8/bin/xc8-ar \
        DFP_DIR=/opt/packs/Microchip/PIC18F-Q_DFP/1.27.449
