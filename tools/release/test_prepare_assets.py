import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

from prepare_assets import prepare


class ReleaseAssetTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.artifacts = self.root / 'artifacts'
        self.manuals = self.artifacts / 'manuals'
        self.manuals.mkdir(parents=True)
        self.output = self.root / 'published'
        for platform in ('macOS', 'Windows', 'Linux'):
            path = self.artifacts / platform
            path.mkdir()
            with zipfile.ZipFile(path / f'Fire-2.1.0-{platform}.zip', 'w') as package:
                package.writestr('plugin-placeholder', 'fixture')
        self.reports = []
        for language in ('en', 'zh-CN', 'zh-TW', 'ja', 'fr', 'es'):
            name = 'Fire-2.1-User-Manual' + ('' if language == 'en' else '-' + language) + '.pdf'
            content = b'%PDF-1.4\n' + language.encode() + b'\n%%EOF'
            (self.manuals / name).write_bytes(content)
            self.reports.append({'language': language, 'file': name,
                                 'sha256': hashlib.sha256(content).hexdigest()})
        with zipfile.ZipFile(self.manuals / 'Fire-2.1-Manuals.zip', 'w') as package:
            for report in self.reports:
                package.write(self.manuals / report['file'], 'Fire-2.1-Manuals/' + report['file'])
            package.writestr('Fire-2.1-Manuals/manual-validation.json',
                             json.dumps({'version': '2.1.0', 'manuals': self.reports}))

    def run_prepare(self):
        return prepare(self.artifacts, self.output, '2.1.0',
                       'The macOS packages are ad-hoc signed and are not Apple-notarized.')

    def test_complete_payload_has_six_pdfs_three_packages_bundle_and_checksums(self):
        self.run_prepare()
        self.assertEqual(len(list(self.output.glob('*.pdf'))), 6)
        self.assertEqual(len(list(self.output.glob('*.zip'))), 4)
        self.assertEqual(len((self.output / 'SHA256SUMS.txt').read_text().splitlines()), 10)

    def test_missing_traditional_manual_blocks_publication(self):
        (self.manuals / 'Fire-2.1-User-Manual-zh-TW.pdf').unlink()
        with self.assertRaisesRegex(ValueError, 'Expected exactly one'):
            self.run_prepare()
        self.assertFalse(self.output.exists())

    def test_duplicate_package_blocks_publication(self):
        duplicate = self.artifacts / 'duplicate'
        duplicate.mkdir()
        (duplicate / 'Fire-2.1.0-Windows.zip').write_bytes(b'duplicate')
        with self.assertRaisesRegex(ValueError, 'found 2'):
            self.run_prepare()

    def test_changed_standalone_pdf_must_match_bundle(self):
        (self.manuals / 'Fire-2.1-User-Manual-fr.pdf').write_bytes(b'%PDF-1.4\nchanged')
        with self.assertRaisesRegex(ValueError, 'manifest mismatch'):
            self.run_prepare()

    def test_unaccepted_notary_receipt_cannot_claim_notarization(self):
        (self.manuals / 'notarization-receipt.json').write_text('{"status":"Invalid"}')
        with self.assertRaisesRegex(ValueError, 'not Accepted'):
            self.run_prepare()

    def test_accepted_notary_receipt_updates_signing_notes(self):
        (self.manuals / 'notarization-receipt.json').write_text('{"status":"Accepted","id":"fixture"}')
        self.assertIn('Developer ID signed and Apple-notarized', self.run_prepare())
        self.assertTrue((self.output / 'notarization-receipt.json').is_file())


if __name__ == '__main__':
    unittest.main()
