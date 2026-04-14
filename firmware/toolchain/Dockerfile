# XC8 is x86-64 only; pin the platform so this works on Apple Silicon too
# (Docker Desktop uses QEMU for x86_64 emulation on arm64 hosts).
FROM --platform=linux/amd64 ubuntu:22.04

# Avoid interactive prompts during apt installs
ENV DEBIAN_FRONTEND=noninteractive

# Enable i386 multiarch so 32-bit XC8 helper binaries can run on the x86-64 image
RUN dpkg --add-architecture i386 \
    && apt-get update && apt-get install -y  --no-install-recommends \
        wget \
        unzip \
        make \
        ca-certificates \
        libc6:i386 \
        libncurses5:i386 \
        libstdc++6:i386 \
    && rm -rf /var/lib/apt/lists/*


# ── XC8 v3.10 ────────────────────────────────────────────────────────────────
# Update URL and SHA256 below if Microchip releases a new installer.
ENV XC8_DIR=/opt/microchip/xc8
ARG XC8_URL="https://ww1.microchip.com/downloads/aemDocuments/documents/DEV/ProductDocuments/SoftwareTools/xc8-v3.10-full-install-linux-x64-installer.run"
ARG XC8_SHA256="628803b96f468a5981d6bc1d0a5e6c7fa809e4d87e3cca961805e2a857f5846e"

RUN wget -q "${XC8_URL}" -O /tmp/xc8-installer.run \
    && echo "${XC8_SHA256}  /tmp/xc8-installer.run" | sha256sum -c -
RUN chmod +x /tmp/xc8-installer.run \
    && /tmp/xc8-installer.run \
        --prefix "${XC8_DIR}" \
        --mode unattended \
        --unattendedmodeui none \
        --netservername localhost \
        --LicenseType FreeMode
RUN test -x /opt/microchip/xc8/bin/xc8-cc \
    && rm /tmp/xc8-installer.run

# ── PIC18F-Q Device Family Pack 1.27.449 ─────────────────────────────────────
ARG DFP_URL="https://packs.download.microchip.com/Microchip.PIC18F-Q_DFP.1.27.449.atpack"
ARG DFP_INSTALL=/opt/packs/Microchip/PIC18F-Q_DFP/1.27.449

RUN mkdir -p "${DFP_INSTALL}" \
    && wget -q "${DFP_URL}" -O /tmp/dfp.atpack \
    && unzip -q /tmp/dfp.atpack -d "${DFP_INSTALL}" \
    && rm /tmp/dfp.atpack

ENV DFP_DIR=/opt/packs/Microchip/PIC18F-Q_DFP/1.27.449

# ── fixDeps stub ──────────────────────────────────────────────────────────────
# fixDeps is an MPLAB X helper that rewrites .d files for incremental builds.
# A no-op stub is sufficient for a clean build inside the container.
RUN printf '#!/bin/sh\nexit 0\n' > /usr/local/bin/fixDeps \
    && chmod +x /usr/local/bin/fixDeps

# ── Build environment ─────────────────────────────────────────────────────────
WORKDIR /project

CMD ["make", "-C", "/dist", "-r", "dist", \
     "CONF=default", \
     "IGNORE_LOCAL=TRUE", \
     "MP_CC=/opt/microchip/xc8/bin/xc8-cc", \
     "MP_CC_DIR=/opt/microchip/xc8/bin/", \
     "MP_AS=/opt/microchip/xc8/bin/xc8-cc", \
     "MP_LD=/opt/microchip/xc8/bin/xc8-cc", \
     "MP_AR=/opt/microchip/xc8/bin/xc8-ar", \
     "DFP_DIR=/opt/packs/Microchip/PIC18F-Q_DFP/1.27.449"]
