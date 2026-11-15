#!/usr/bin/env python3
"""Require complete, consistent release assets before publishing a tag."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile

LANGUAGES = ('en', 'zh-CN', 'zh-TW', 'ja', 'fr', 'es')


def sha256(path):
    hasher = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            hasher.update(block)
    return hasher.hexdigest()


def prepare(artifacts, destination, version, notes):
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Expected a numeric stable release version')
    series = '.'.join(version.split('.')[:2])
    pdf_names = [f'Fire-{series}-User-Manual' + ('' if lang == 'en' else f'-{lang}') + '.pdf'
                 for lang in LANGUAGES]
    bundle_name = f'Fire-{series}-Manuals.zip'
    expected = [f'Fire-{version}-{platform}.zip' for platform in ('macOS', 'Windows', 'Linux')]
    expected += pdf_names + [bundle_name]
    assets = {}
    for name in expected:
        matches = [path for path in artifacts.glob(f'*/{name}') if path.is_file()]
        if len(matches) != 1:
            raise ValueError(f'Expected exactly one {name}; found {len(matches)}')
        path = matches[0]
        if path.suffix == '.zip':
            with zipfile.ZipFile(path) as archive:
                if not archive.namelist() or archive.testzip() is not None:
                    raise ValueError(f'Invalid ZIP: {name}')
        elif not path.read_bytes().startswith(b'%PDF-'):
            raise ValueError(f'Invalid PDF: {name}')
        assets[name] = path
    prefix = f'Fire-{series}-Manuals/'
    with zipfile.ZipFile(assets[bundle_name]) as bundle:
        manifest = json.loads(bundle.read(prefix + 'manual-validation.json'))
        if manifest.get('version') != version:
            raise ValueError('Manual bundle version differs from release version')
        reports = manifest.get('manuals', [])
        if len(reports) != 6 or {item['language'] for item in reports} != set(LANGUAGES):
            raise ValueError('Manual bundle is missing a language')
        for item in reports:
            name = item['file']
            if name not in pdf_names or item['sha256'] != sha256(assets[name]):
                raise ValueError(f'Manual manifest mismatch: {name}')
            if hashlib.sha256(bundle.read(prefix + name)).hexdigest() != item['sha256']:
                raise ValueError(f'Bundled and standalone PDF differ: {name}')
    receipts = list(artifacts.glob('*/notarization-receipt.json'))
    if len(receipts) > 1:
        raise ValueError('Multiple notarization receipts found')
    if receipts:
        receipt = json.loads(receipts[0].read_text())
        if receipt.get('status') != 'Accepted':
            raise ValueError('Notarization receipt is not Accepted')
        assets['notarization-receipt.json'] = receipts[0]
        notes = notes.replace('The macOS packages are ad-hoc signed and are not Apple-notarized.',
                              'The macOS packages are Developer ID signed and Apple-notarized.')
    if destination.exists() and any(destination.iterdir()):
        raise ValueError('Release staging directory must be empty')
    destination.mkdir(parents=True, exist_ok=True)
    for name, path in assets.items():
        shutil.copy2(path, destination / name)
    sums = [f'{sha256(destination / name)}  {name}' for name in sorted(assets)]
    (destination / 'SHA256SUMS.txt').write_text('\n'.join(sums) + '\n', encoding='utf-8')
    return notes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts-root', type=Path, default=Path('.'))
    parser.add_argument('--destination', type=Path, default=Path('release-assets'))
    parser.add_argument('--version-file', type=Path, default=Path('VERSION'))
    parser.add_argument('--notes', type=Path, required=True)
    parser.add_argument('--body-output', type=Path, required=True)
    args = parser.parse_args()
    notes = prepare(args.artifacts_root, args.destination,
                    args.version_file.read_text().strip(), args.notes.read_text(encoding='utf-8'))
    args.body_output.write_text(notes, encoding='utf-8')
    print(f'Prepared {len(list(args.destination.iterdir()))} verified release assets')


if __name__ == '__main__':
    main()
