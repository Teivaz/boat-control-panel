# PIC18-Q84 Data Sheet — offline mirror (Rev. F)

Local copy of the HTML web-help edition of the PIC18F27/47/57Q84 data sheet,
fetched from Microchip onlinedocs on 2026-09-04.

- **Source:** <https://onlinedocs.microchip.com/oxy/GUID-BA964748-63AC-4D4B-AD5F-E96EAA995DF8-en-US-22/index.html>
- **Title:** *28/40/44/48-Pin, Low-Power, High-Performance Microcontroller with XLP Technology*
- **Devices:** PIC18F27Q84 / PIC18F47Q84 / PIC18F57Q84
- **Revision:** **F, 12/2023** (per chapter 53, *Appendix A: Revision History*)
- **Contents:** 1,649 HTML topics, 270 figures (SVG/PNG) — 54 MB

The WebHelp chrome has been stripped by
[`../../tools/clean_datasheet_mirror.py`](../../tools/clean_datasheet_mirror.py):
the site header, search form, colour stripes, the full publication TOC that was
repeated on every page, the disclaimer, corporate footer, modals and all
scripts and analytics are gone. Topic HTML went from 133 MB to 19.6 MB (14.7% of
the original) with content, tables and figures untouched.

Each page now carries only:

- the topic title (`<h1>`) and content
- a breadcrumb showing the ancestor chain
- previous / next sibling links, also as `<link rel="prev|next">`
- a **Subtopics** list of children, plus `<link rel="up">` to the parent
- one shared `style.css` (light/dark, readable offline — no vendor CSS)

Re-run the cleaner any time; it is idempotent on already-cleaned input only in
the sense that it needs the *raw* mirror, so re-download first if you want to
change the output format.

## Navigating

Open `index.html` — a single nested list of the whole 1,649-topic tree.

`pages.json` is the machine-readable index: for every file, its `title`,
`parent`, `children`, `prev`, `next` and full `breadcrumb`. That is the quick
way to find a section by number:

```sh
python3 -c "
import json
m=json.load(open('pages.json'))
for f,v in m.items():
    if v['title'].startswith('37.5'): print(v['title'],'->',f)
"
```

The hierarchy was reconstructed from the breadcrumbs of all pages, since the
WebHelp ships no TOC tree file and `index.html` upstream only listed two levels.
182 register topics (`TRISB`, `ANSELE`, `PORTB`, `UyCTSPPS`, …) carry no
breadcrumb at all — the publisher generated them but never placed them in the
TOC, and they link only to each other. They are real content (only 25 duplicate
a numbered section by title, and not even those have identical bodies), so they
are collected under [`unfiled-registers.html`](unfiled-registers.html) rather
than dropped or left cluttering the top level.

## Relationship to the PDF we already had

`../../../PIC18F27-47-57Q84-Data-Sheet-40002213D.pdf` is **Rev. D (04/2021)** —
two revisions older. Per the revision history, the deltas are:

| Rev | Date | Changes |
|---|---|---|
| F | 12/2023 | DC/AC characteristics graphs added; Flash endurance, VQFN dimensions, CAN FD max data bit rate + example corrected; grammar |
| E | 12/2021 | Product Identification System packaging fixed (28-lead VQFN, 40-lead VQFN added); editorial |

**Neither E nor F touches the I²C or DMA chapters.** Verified directly: §37.3.7,
§37.5.12, §16.3.3 and §16.9 are unchanged in wording from Rev D.

## Why this mirror exists

It was fetched to check whether a newer revision resolves the datasheet
self-contradictions catalogued in
[`../../firmware/I2C_complete_reference_guide.md`](../../firmware/I2C_complete_reference_guide.md)
§A. **It does not** — most importantly the `TOBY32` polarity conflict is
reproduced verbatim in Rev. F:

- §37.3.7 prose: *"If the TOBY32 bit is set (TOBY32 = 1), the time-out period
  determined by the TOTIME bits is multiplied by 32."*
- Both worked examples agree (the 64 ms one sets `TOBY32 = 1` with the comment
  `BTO time = TOTIME * T_BTOCLK * 32`).
- §37.5.12 register table says the **opposite**: `TOBY32 = 1` → `TOTIME × T_BTOCLK`,
  `TOBY32 = 0` → `TOTIME × T_BTOCLK × 32`.

So the ambiguity is genuine and current, not a stale-revision artifact. It still
has to be settled on the bench — see assumption **C1** in
`tools/i2c_assumption_tests.py` for the measurement procedure.

## Not in git

This directory is gitignored: it is 54 MB of third-party documentation, and the
online copies carry Microchip's own caveat that they are *"provided as a
courtesy"* and that content should be verified against the PDF on the device
product page. It is fully re-creatable: crawl the source URL transitively from
`index.html` until closure, then run `tools/clean_datasheet_mirror.py` on the
result.
