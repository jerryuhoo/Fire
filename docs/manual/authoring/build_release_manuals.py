#!/usr/bin/env python3
"""Build and validate all release PDFs and their portable source bundle."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time
from urllib.error import URLError
from urllib.request import urlopen
import zipfile

from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
from pypdf import PdfReader

from manual_locales import LABELS

AUTHORING = Path(__file__).resolve().parent
MANUAL = AUTHORING.parent
ROOT = MANUAL.parent.parent
LANGUAGES = ('en', 'zh-CN', 'zh-TW', 'ja', 'fr', 'es')
FONT_FAMILIES = {'zh-CN': 'notosanssc', 'zh-TW': 'notosanstc', 'ja': 'notosansjp'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cached_download(asset, directory):
    path = directory / asset['file']
    if path.is_file() and digest(path) == asset['sha256']:
        return path
    for attempt in range(3):
        try:
            with urlopen(asset['url'], timeout=45) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != asset['sha256']:
                raise ValueError(f"Checksum mismatch: {asset['file']}")
            temporary = path.with_suffix(path.suffix + '.part')
            temporary.write_bytes(data)
            temporary.replace(path)
            return path
        except (OSError, URLError):
            if attempt == 2:
                raise
            time.sleep(attempt + 1)
    raise RuntimeError('Download retry limit exceeded')


def prepare_fonts(directory):
    directory.mkdir(parents=True, exist_ok=True)
    fonts, licenses = {}, []
    for entry in json.loads((AUTHORING / 'fonts.json').read_text()):
        variable = cached_download(entry['font'], directory)
        licenses.append(cached_download(entry['license'], directory))
        paths = []
        source = None
        for weight in (400, 700):
            path = directory / f"{entry['family']}-{entry['font']['sha256'][:12]}-{weight}.ttf"
            if not path.exists():
                if source is None:
                    source = TTFont(variable)
                face = instantiateVariableFont(source, {'wght': weight},
                                               inplace=False, updateFontNames=True)
                temporary = path.with_suffix('.part')
                face.save(temporary)
                face.close()
                temporary.replace(path)
            paths.append(path)
        if source is not None:
            source.close()
        fonts[entry['family']] = paths
        print(f"Prepared {entry['family']} fonts", flush=True)
    return fonts, licenses


def heading_slug(text):
    plain = re.sub(r'[*`_]', '', text).strip()
    return re.sub(r'[^\w\s-]', '', plain.lower()).strip().replace(' ', '-')


def validate_source(source, language, version):
    text = source.read_text(encoding='utf-8')
    chapters = re.findall(r'^## (\d{2})\.', text, re.MULTILINE)
    if chapters != [f'{n:02d}' for n in range(1, 23)]:
        raise ValueError(f'{source.name}: expected chapters 01-22 in order')
    if version not in LABELS[language]['edition'] or version not in text:
        raise ValueError(f'{source.name}: manual edition does not match VERSION')
    headings = {heading_slug(h) for h in re.findall(r'^#{1,6} (.+)$', text, re.MULTILINE)}
    for anchor in re.findall(r'\[[^\]]+\]\(#([^)]+)\)', text):
        if anchor not in headings:
            raise ValueError(f'{source.name}: broken contents link #{anchor}')
    images = re.findall(r'!\[[^\]]*\]\(([^)]+)\)', text)
    if len(images) != 23:
        raise ValueError(f'{source.name}: expected 23 illustrated figures')
    for image in images:
        if not (source.parent / image).is_file():
            raise ValueError(f'{source.name}: missing image {image}')
    if '[FIGURE:' in text or re.search(r'\bTODO\b|\bTBD\b', text):
        raise ValueError(f'{source.name}: editorial placeholder remains')


def validate_pdf(path, language, version):
    reader = PdfReader(path)
    if not 35 <= len(reader.pages) <= 90:
        raise ValueError(f'{path.name}: unexpected page count {len(reader.pages)}')
    if reader.metadata.title != LABELS[language]['pdf_title']:
        raise ValueError(f'{path.name}: wrong PDF title')
    if version not in reader.pages[0].extract_text():
        raise ValueError(f'{path.name}: release version missing from cover')
    chapters = []

    def visit(outline):
        for item in outline:
            if isinstance(item, list):
                visit(item)
            else:
                page = reader.get_destination_page_number(item)
                if page is None or not 0 <= page < len(reader.pages):
                    raise ValueError(f'{path.name}: invalid bookmark destination')
                match = re.match(r'^(\d{2})\.', item.title)
                if match:
                    chapters.append(match[1])

    visit(reader.outline)
    if chapters != [f'{n:02d}' for n in range(1, 23)]:
        raise ValueError(f'{path.name}: incomplete chapter bookmarks')
    for number, page in enumerate(reader.pages, 1):
        text = page.extract_text()
        if not text.strip() or '\ufffd' in text or '\x00' in text:
            raise ValueError(f'{path.name}: empty or damaged text on page {number}')
    return {'language': language, 'file': path.name, 'pages': len(reader.pages),
            'sha256': digest(path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'output/pdf')
    parser.add_argument('--font-dir', type=Path, default=ROOT / 'tmp/pdfs/fonts')
    args = parser.parse_args()
    version = (ROOT / 'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Manual exports require a numeric release VERSION')
    series = '.'.join(version.split('.')[:2])
    args.output_dir.mkdir(parents=True, exist_ok=True)
    sources = {language: MANUAL / (f'Fire-{series}-User-Manual'
               + ('' if language == 'en' else f'-{language}') + '.md')
               for language in LANGUAGES}
    for language, source in sources.items():
        validate_source(source, language, version)
    fonts, licenses = prepare_fonts(args.font_dir)
    reports, pdfs = [], []
    for language, source in sources.items():
        pdf = args.output_dir / (source.stem + '.pdf')
        command = [sys.executable, str(AUTHORING / 'build_pdf.py'), '--language', language,
                   '--source', str(source), '--output', str(pdf)]
        if language in FONT_FAMILIES:
            regular, bold = fonts[FONT_FAMILIES[language]]
            command += ['--cjk-regular', str(regular), '--cjk-bold', str(bold), '--cjk-index', '0']
        subprocess.run(command, check=True)
        reports.append(validate_pdf(pdf, language, version))
        pdfs.append(pdf)
    manifest = args.output_dir / 'manual-validation.json'
    manifest.write_text(json.dumps({'version': version, 'manuals': reports}, indent=2) + '\n')
    bundle = args.output_dir / f'Fire-{series}-Manuals.zip'
    prefix = f'Fire-{series}-Manuals/'
    with zipfile.ZipFile(bundle, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for path in [*pdfs, *sources.values()]:
            archive.write(path, prefix + path.name)
        for path in sorted((MANUAL / 'images').glob('*.png')):
            archive.write(path, prefix + 'images/' + path.name)
        archive.write(AUTHORING / 'screenshot-manifest.json', prefix + 'screenshot-manifest.json')
        for path in licenses:
            archive.write(path, prefix + 'licenses/' + path.name)
        archive.write(manifest, prefix + manifest.name)
        archive.writestr(prefix + 'READ-ME.txt',
                        f'Fire {version} user manuals in six languages.\n'
                        'Open a PDF, or keep the Markdown files beside the images folder.\n'
                        'Screenshot provenance and embedded-font licenses are included.\n')
    with zipfile.ZipFile(bundle) as archive:
        if archive.testzip() is not None:
            raise ValueError('Manual ZIP integrity check failed')
    print(json.dumps({'version': version, 'manuals': reports, 'bundle': str(bundle)}, indent=2))


if __name__ == '__main__':
    main()
