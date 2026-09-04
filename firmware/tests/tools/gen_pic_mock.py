#!/usr/bin/env python3
"""
gen_pic_mock.py — generate tests/mocks/pic_sfr_mock.h from the real XC8
device header.

The firmware talks to the PIC18F27Q84 through `<xc.h>`, whose registers are
`extern volatile ... __at(0xNNN)` declarations placed at fixed addresses by
the linker.  For host-side unit tests we want the same names, the same bit
layouts and the same *aliasing* (writing `LATA` must be visible through
`LATAbits.LATA0`), but backed by ordinary memory the test can poke.

So: parse the device header, keep only the registers the firmware actually
mentions, and re-emit each one as a macro that dereferences a pointer into a
single backing array.  `LATA` and `LATAbits` then resolve to the same byte,
exactly as on silicon.

Two adjustments are made to the bitfield unions:

  * `unsigned x : 1` becomes `uint8_t x : 1`.  XC8's `unsigned` is 16 bits and
    it allocates bitfields LSB-first within a byte; a host compiler's 32-bit
    `unsigned` would make the union 4 bytes wide and let a single-bit write
    clobber the three following registers.
  * the union is marked packed, pinning it to one byte.

Registers are *not* special-cased here — the routing of DMA banking and the
NVM state machine happens in pic_mock.h's `pic_reg()`, which every generated
macro goes through.

Usage:
    ./gen_pic_mock.py [--header PATH] [--out PATH]

The generated file is committed, so running this is only necessary after the
firmware starts using a register it did not use before (the test build will
fail with an "undeclared identifier" naming it).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIRMWARE = HERE.parent.parent

DEFAULT_HEADER_GLOBS = [
    "/Applications/microchip/mplabx/*/packs/Microchip/PIC18F-Q_DFP/*/xc8/pic/include/proc/pic18f27q84.h",
    "/opt/microchip/mplabx/*/packs/Microchip/PIC18F-Q_DFP/*/xc8/pic/include/proc/pic18f27q84.h",
]

# Sources scanned to decide which registers to keep.  u8g2 is portable C and
# never touches an SFR, so it is skipped.
SCAN_GLOBS = [
    "board1-switching/*.c", "board1-switching/*.h",
    "board2-buttons/*.c", "board2-buttons/*.h",
    "board3-main/*.c", "board3-main/*.h",
    "libcomm/*.c", "libcomm/*.h",
    "libcomm/alt/*.c", "libcomm/alt/*.h",
]

TYPE_MAP = {
    "unsigned char": "uint8_t",
    "unsigned short": "uint16_t",
    "unsigned int": "uint16_t",
    "unsigned short long": "uint24_t",
    "__uint24": "uint24_t",
    "unsigned long": "uint32_t",
}

RE_EXTERN = re.compile(
    r"^extern\s+volatile\s+(?P<type>__uint24|unsigned(?:\s+\w+)*?)\s+"
    r"(?P<name>[A-Za-z_]\w*)\s+__at\((?P<addr>0x[0-9A-Fa-f]+)\);"
)
RE_EXTERN_BITS = re.compile(
    r"^extern\s+volatile\s+(?P<name>[A-Za-z_]\w*)bits_t\s+(?P=name)bits\s+"
    r"__at\((?P<addr>0x[0-9A-Fa-f]+)\);"
)
RE_UNION_START = re.compile(r"^typedef union\s*\{")
RE_UNION_END = re.compile(r"^\}\s*(?P<name>[A-Za-z_]\w*)bits_t;")
RE_DEFINE_BIT = re.compile(r"^#define\s+_(?P<reg>[A-Za-z_]\w*?)_(?P<rest>\w+)\s+(?P<val>0x[0-9A-Fa-f]+|\d+)\s*$")
# Plain address constants, e.g. `#define DIA_FVRA2X 0x2C0032` — adc.c table-reads
# the factory FVR calibration through one of these.
RE_DEFINE_CONST = re.compile(r"^#define\s+(?P<name>[A-Za-z]\w*)\s+(?P<val>0x[0-9A-Fa-f]+)\s*$")
RE_IDENT = re.compile(r"[A-Za-z_]\w*")


class Register:
    __slots__ = ("name", "ctype", "addr", "union", "defines")

    def __init__(self, name, ctype, addr):
        self.name = name
        self.ctype = ctype
        self.addr = addr
        self.union = None      # list of source lines for the bitfield union
        self.defines = []      # ["_LATA_LATA0_POSN", "0x0"] pairs


def parse_header(path: Path) -> tuple[dict[str, Register], dict[str, str]]:
    regs: dict[str, Register] = {}
    consts: dict[str, str] = {}
    lines = path.read_text(errors="replace").splitlines()

    pending_union: list[str] | None = None
    for line in lines:
        if pending_union is not None:
            m = RE_UNION_END.match(line)
            pending_union.append(line)
            if m:
                name = m.group("name")
                if name in regs:
                    regs[name].union = pending_union
                pending_union = None
            continue

        if RE_UNION_START.match(line):
            pending_union = [line]
            continue

        m = RE_EXTERN.match(line)
        if m:
            ctype = " ".join(m.group("type").split())
            name = m.group("name")
            if name not in regs:
                regs[name] = Register(name, ctype, int(m.group("addr"), 16))
            continue

        m = RE_EXTERN_BITS.match(line)
        if m:
            # Address of the bits view; the byte view is normally declared just
            # above with the same address, but a few registers only have bits.
            name = m.group("name")
            if name not in regs:
                regs[name] = Register(name, "unsigned char", int(m.group("addr"), 16))
            continue

        m = RE_DEFINE_BIT.match(line)
        if m:
            reg = m.group("reg")
            if reg in regs:
                regs[reg].defines.append((f"_{reg}_{m.group('rest')}", m.group("val")))
            continue

        m = RE_DEFINE_CONST.match(line)
        if m and m.group("name") not in regs:
            consts[m.group("name")] = m.group("val")

    return regs, consts


def scan_used_identifiers(root: Path) -> set[str]:
    used: set[str] = set()
    for pattern in SCAN_GLOBS:
        for path in sorted(root.glob(pattern)):
            text = path.read_text(errors="replace")
            used.update(RE_IDENT.findall(text))
    return used


def fix_union(lines: list[str]) -> str:
    """Retype `unsigned` bitfields to uint8_t and pack the union to one byte."""
    out = []
    for line in lines:
        if RE_UNION_START.match(line):
            out.append("typedef union {")
            continue
        m = RE_UNION_END.match(line)
        if m:
            out.append("} __attribute__((packed)) %sbits_t;" % m.group("name"))
            continue
        # "        unsigned NAME                  :1;"  /  "unsigned  :2;"
        stripped = line.strip()
        if stripped.startswith("unsigned"):
            body = stripped[len("unsigned"):]
            out.append("    uint8_t" + body)
        elif stripped.startswith("struct {"):
            out.append("    struct {")
        elif stripped == "};":
            out.append("    } __attribute__((packed));")
        else:
            out.append("    " + stripped)
    return "\n".join(out)


def emit(regs: dict[str, Register], consts: dict[str, str], used: set[str], out_path: Path,
         header_path: Path) -> int:
    selected = []
    for name, reg in sorted(regs.items(), key=lambda kv: (kv[1].addr, kv[0])):
        if name in used or (name + "bits") in used:
            selected.append(reg)

    body = []
    body.append("/* GENERATED by tests/tools/gen_pic_mock.py — do not edit by hand.")
    body.append(" *")
    body.append(" * Source: %s" % header_path)
    body.append(" *")
    body.append(" * Only the %d registers the firmware actually references are emitted." % len(selected))
    body.append(" * Every access routes through pic_reg(), so DMA banking and the NVM")
    body.append(" * state machine in pic_mock.c see it. */")
    body.append("")
    body.append("#ifndef PIC_SFR_MOCK_H")
    body.append("#define PIC_SFR_MOCK_H")
    body.append("")
    body.append('#include "pic_mock.h"')
    body.append("")
    body.append("/* Bitfield layouts first, register macros second.  A few registers name a")
    body.append(" * bitfield after the register itself (NVMADRL holds NVMADRL[7:0]); emitting")
    body.append(" * the #define first would expand it inside its own union. */")
    body.append("")

    for reg in selected:
        if reg.union is not None:
            body.append("/* %s @ 0x%03X */" % (reg.name, reg.addr))
            body.append(fix_union(reg.union))
            body.append("")

    for reg in selected:
        ctype = TYPE_MAP.get(reg.ctype)
        if ctype is None:
            print("warning: unmapped type %r for %s" % (reg.ctype, reg.name), file=sys.stderr)
            ctype = "uint8_t"
        body.append("/* ---- %s @ 0x%03X ---- */" % (reg.name, reg.addr))
        # uint24_t is pointer-width on the host, so an 8-byte write would run
        # over the registers above it; those live in side storage instead.
        accessor = "pic_ptrreg" if ctype == "uint24_t" else "pic_reg"
        body.append("#define %s (*(volatile %s*)%s(0x%03Xu))" % (reg.name, ctype, accessor, reg.addr))
        if reg.union is not None:
            body.append("#define %sbits (*(volatile %sbits_t*)pic_reg(0x%03Xu))"
                        % (reg.name, reg.name, reg.addr))
        for macro, value in reg.defines:
            body.append("#define %s %s" % (macro, value))
        body.append("")

    selected_consts = sorted(n for n in consts if n in used and n not in regs)
    if selected_consts:
        body.append("/* ---- Address constants ---- */")
        for name in selected_consts:
            body.append("#define %s %s" % (name, consts[name]))
        body.append("")

    body.append("#endif /* PIC_SFR_MOCK_H */")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(body) + "\n")
    return len(selected)


def find_header() -> Path | None:
    import glob
    for pattern in DEFAULT_HEADER_GLOBS:
        matches = sorted(glob.glob(pattern))
        if matches:
            return Path(matches[-1])
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--header", type=Path, default=None,
                    help="path to pic18f27q84.h from the XC8 DFP")
    ap.add_argument("--out", type=Path, default=HERE.parent / "mocks" / "pic_sfr_mock.h")
    args = ap.parse_args()

    header = args.header or find_header()
    if header is None or not header.exists():
        print("error: could not locate pic18f27q84.h; pass --header", file=sys.stderr)
        print("       (the generated header is committed, so this is only needed", file=sys.stderr)
        print("        when the firmware starts using a new register)", file=sys.stderr)
        return 2

    regs, consts = parse_header(header)
    used = scan_used_identifiers(FIRMWARE)
    count = emit(regs, consts, used, args.out, header)
    print("wrote %s (%d registers of %d in the device header)" % (args.out, count, len(regs)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
