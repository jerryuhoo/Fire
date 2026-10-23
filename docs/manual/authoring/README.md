# Building the illustrated manual

The canonical English text is `../Fire-2.1-User-Manual.md`. Image paths are
relative to that file. `build_pdf.py` uses the same Markdown to produce
`output/pdf/Fire-2.1-User-Manual.pdf` from the repository root.
Complete translations use the suffixes `-zh-CN`, `-ja`, `-fr`, and `-es` for
Simplified Chinese, Japanese, French, and Spanish. All five editions share the
same genuine screenshots and retain English UI labels for reference.

## PDF

Install `reportlab` and `Pillow` in your Python environment, then run:

```sh
python3 docs/manual/authoring/build_pdf.py
python3 docs/manual/authoring/build_pdf.py --language zh-CN
python3 docs/manual/authoring/build_pdf.py --language fr
python3 docs/manual/authoring/build_pdf.py --language es
python3 docs/manual/authoring/build_pdf.py --language ja \
  --cjk-regular /path/to/NotoSansJP-Regular.ttf \
  --cjk-bold /path/to/NotoSansJP-Bold.ttf --cjk-index 0
```

The PDF embeds the ReportLab-distributed Vera text fonts and uses standard
Courier for code. The Chinese edition embeds subsets of macOS's STHeiti SC
Light and Medium fonts (collection face 1) and uses Chinese text wrapping.
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

- Product source: `9ea20d2` (Fire 2.1.0 development edition).
- Prepared: 24 October 2026.
- Scope: 22 chapters, all current effect families, extended modulation,
  parallel/Mid-Side routing, state/library operations and sample import/export.
- Source review distinguished original and independently inserted module
  ranges, the native Serial Shape compatibility behavior, A/B compensation
  persistence, and raw frozen-audio export.
- The PDF was checked for missing images, table/paragraph overflow, text
  extraction, bookmark destinations and visible page defects.

When updating the version, revise the edition metadata in the Markdown and PDF
builder, regenerate affected screenshots, then rebuild and inspect the PDF.
