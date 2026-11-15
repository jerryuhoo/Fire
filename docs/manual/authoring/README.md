# Building the illustrated manual

The canonical English text is `../Fire-2.1-User-Manual.md`. Image paths are
relative to that file. `build_pdf.py` uses the same Markdown to produce
`output/pdf/Fire-2.1-User-Manual.pdf` from the repository root.
Complete translations use the suffixes `-zh-CN`, `-zh-TW`, `-ja`, `-fr`, and `-es` for
Simplified Chinese, Traditional Chinese, Japanese, French, and Spanish. All six editions share the
same genuine screenshots and retain English UI labels for reference.

## Release bundle

Use Python 3.12 and the pinned dependencies to build all six PDFs and a ZIP
containing the PDFs, Markdown, screenshots and font licenses:

```sh
python3 -m pip install -r docs/manual/authoring/requirements.txt
python3 docs/manual/authoring/build_release_manuals.py
```

The release builder downloads Noto Sans SC, TC and JP from the immutable Google
Fonts revision recorded in `fonts.json`, verifies each SHA-256, and creates static
Regular/Bold faces with the pinned fonttools version. The same fonts and package
versions are used locally and on Linux CI. Font binaries stay in `tmp/pdfs/fonts`;
they are not committed. Optional `--font-dir` and `--output-dir` arguments select
other cache and export directories.

Before packaging, every edition must have chapters 01-22, all 23 screenshot
references, valid contents links, matching release metadata, readable PDF text
and working chapter bookmarks. The builder emits `manual-validation.json` with
page counts and PDF hashes. Visual inspection is still required before release.

The Actions `User manuals` job uploads the exports as `Fire-User-Manuals`.
The tag release job requires that job and all platform builds to succeed. It
rejects missing/duplicate packages or languages and requires the standalone PDFs
to match the copies and hashes in the manual ZIP. The final assets include
`SHA256SUMS.txt`. The workflow creates a draft release; inspect the uploaded
packages and manuals before publishing it as the stable release. PDFs and
export bundles remain outside Git.

## PDF

Install `reportlab` and `Pillow` in your Python environment, then run:

```sh
python3 docs/manual/authoring/build_pdf.py
python3 docs/manual/authoring/build_pdf.py --language zh-CN
python3 docs/manual/authoring/build_pdf.py --language zh-TW
python3 docs/manual/authoring/build_pdf.py --language fr
python3 docs/manual/authoring/build_pdf.py --language es
python3 docs/manual/authoring/build_pdf.py --language ja \
  --cjk-regular /path/to/NotoSansJP-Regular.ttf \
  --cjk-bold /path/to/NotoSansJP-Bold.ttf --cjk-index 0
```

The PDF embeds the ReportLab-distributed Vera text fonts and uses standard
Courier for code. A standalone Chinese export on macOS defaults to STHeiti
Light and Medium: face 1 for Simplified Chinese, face 0 for Traditional Chinese.
Both use Chinese text wrapping. Release exports use the pinned Noto fonts above.
On another system, supply embeddable Chinese TrueType fonts with
`--cjk-regular /path/to/regular.ttf --cjk-bold /path/to/bold.ttf --cjk-index 0`.
The Japanese edition uses Japanese glyphs from Noto Sans JP Regular and Bold.
Obtain the variable TrueType source and its SIL Open Font License from the
[official Google Fonts repository](https://github.com/google/fonts/tree/main/ofl/notosansjp).
With the optional `fonttools` package installed, create static weights outside
the repository using the following Python code (adjust the source path):

```python
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

source = TTFont('/path/to/NotoSansJP[wght].ttf')
for name, weight in [('Regular', 400), ('Bold', 700)]:
    face = instantiateVariableFont(source, {'wght': weight}, inplace=False,
                                   updateFontNames=True)
    face.save(f'/path/to/NotoSansJP-{name}.ttf')
```

Font binaries are not copied into the repository. The builder checks every
edition for missing glyphs before export. Localized cover and navigation labels
live in `manual_locales.py`. Japanese paragraphs use `japanese_layout.py` to
preserve styled text while enforcing prohibited line-start/end characters.
All editions include a linked contents page and chapter/section
bookmarks. The `manual:body` comment separates the Markdown contents from the
PDF body; `figure:landscape` requests a wide screenshot page. The optional
`figure:max-height=215` comment caps the next screenshot's height in PDF points,
and `table:keep` keeps the next short table together. Localized pagination may
use these comments without changing the Markdown reading order. The Japanese
and Spanish tape illustration uses an existing genuine close-up capture so its
details remain readable alongside the longer translated model descriptions.
Re-render the PDF
with Poppler and inspect the pages before publishing an updated edition.
Exported PDFs and portable ZIP bundles are ignored by Git; commit the Markdown,
image sources, and authoring tools instead.

## Genuine interface captures

The opt-in `tests/ManualScreenshotCapture.cpp` fixture paints real product
components and feeds generated audio through the processor. It does not use a
user DAW session or user audio. It isolates its preset library in a temporary
directory and restores the appearance preference afterward. Without the
environment variable below, it skips. No production DSP or UI source is changed
by the capture operation.

```sh
cmake --build Builds/JUCE9-EqDev --target Tests --parallel 4
FIRE_MANUAL_SCREENSHOT_DIR="$PWD/docs/manual/images" \
  Builds/JUCE9-EqDev/Tests '[.manual-screenshots]'
```

The selected-file Granular import preview comes from the existing real
decode-worker UI fixture. It creates a temporary WAV and captures the ready
preview before the fixture's cancellation checks:

```sh
FIRE_GRANULAR_MEDIA_PREVIEW_DIR=/tmp/fire-manual-import-preview \
  Builds/JUCE9-EqDev/Tests \
  'Sample popover remains within a 1000x500 host and cancels on Granular rebinding in all skins'
cp /tmp/fire-manual-import-preview/ink-granular-sample.png \
  docs/manual/images/12-granular-import.png
```

These GUI fixtures need access to the desktop display service. On macOS, a
restricted/headless environment may not provide a display even though the
plug-in itself is working. They do not launch or restart Live.

All screenshots in this edition are original product renders. No controls,
readouts, or lighting were added with image generation or painted over. The
standalone settings capture truthfully identifies the test host. Some unused
full-window/detail variants are retained in `images/` for future documentation.

## Edition and validation

- Manual edition: Fire 2.1.0 release, updated 15 November 2026.
- Screenshot source: `9ea20d2`, captured 24 October 2026. These genuine captures
  remain applicable to the release; their original provenance is retained in
  `screenshot-manifest.json`.
- Scope: 22 chapters, all current effect families, extended modulation,
  parallel/Mid-Side routing, state/library operations and sample import/export.
- Source review distinguished original and independently inserted module
  ranges, the native Serial Shape compatibility behavior, A/B compensation
  persistence, and raw frozen-audio export.
- The PDF was checked for missing images, table/paragraph overflow, text
  extraction, bookmark destinations and visible page defects.

When updating the version, revise the edition metadata in the Markdown and PDF
builder, regenerate affected screenshots, then rebuild and inspect the PDF.
