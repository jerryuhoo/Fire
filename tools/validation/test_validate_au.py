import contextlib
import io
from pathlib import Path
import plistlib
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

import validate_au


class AuValidationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.bundle = self.root / 'Fire.component'
        binary = self.bundle / 'Contents/MacOS/Fire'
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b'fixture')
        self.info = {'CFBundleExecutable': 'Fire', 'CFBundleVersion': '2.1.0',
                     'CFBundleShortVersionString': '2.1.0',
                     'AudioComponents': [{'name': 'Blue Wings Music: Fire', 'type': 'aufx',
                                          'subtype': 'Fire', 'manufacturer': 'Wing', 'version': 131328}]}
        self.save_manifest()
        self.metadata = validate_au.component_metadata(self.bundle, '2.1.0')

    def save_manifest(self):
        (self.bundle / 'Contents/Info.plist').write_bytes(plistlib.dumps(self.info))

    def test_manifest_checks_version_and_exact_component_identity(self):
        self.assertEqual(self.metadata['identifier'], 'AudioUnit:Effects/aufx,Fire,Wing')
        for key, value in [('version', 131072), ('subtype', 'Other'), ('type', 'aumf')]:
            with self.subTest(key=key):
                original = self.info['AudioComponents'][0][key]
                self.info['AudioComponents'][0][key] = value
                self.save_manifest()
                with self.assertRaises(ValueError):
                    validate_au.component_metadata(self.bundle, '2.1.0')
                self.info['AudioComponents'][0][key] = original

    def test_missing_registration_and_stale_version_are_retried_in_fresh_processes(self):
        replies = [subprocess.CompletedProcess([], code, 'registry state', '') for code in (2, 3, 0)]
        with patch.object(validate_au.subprocess, 'run', side_effect=replies) as run, \
                patch.object(validate_au.time, 'sleep') as sleep, contextlib.redirect_stdout(io.StringIO()):
            validate_au.wait_for_registration(Path('/probe'), self.metadata, self.root / 'registry.log', 60)
        self.assertEqual(run.call_count, 3)
        self.assertEqual(sleep.call_count, 2)
        self.assertEqual(run.call_args.args[0], ['/probe', 'aufx', 'Fire', 'Wing', '131328'])

    def test_registry_wait_has_a_deadline(self):
        clock = [0.0]
        def sleep(seconds):
            clock[0] += seconds
        missing = subprocess.CompletedProcess([], 2, 'AU not registered', '')
        with patch.object(validate_au.subprocess, 'run', return_value=missing), \
                patch.object(validate_au.time, 'monotonic', side_effect=lambda: clock[0]), \
                patch.object(validate_au.time, 'sleep', side_effect=sleep), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(RuntimeError, 'did not appear'):
                validate_au.wait_for_registration(Path('/probe'), self.metadata, self.root / 'registry.log', 4)
        self.assertEqual(clock[0], 4)

    def test_probe_errors_are_not_treated_as_registration_delays(self):
        failed = subprocess.CompletedProcess([], 1, '', 'native registry error')
        with patch.object(validate_au.subprocess, 'run', return_value=failed) as run, contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(RuntimeError, 'native registry error'):
                validate_au.wait_for_registration(Path('/probe'), self.metadata, self.root / 'registry.log', 60)
        self.assertEqual(run.call_count, 1)

    def valid_log(self):
        return 'Num plugins found: 1\nBlue Wings Music: Fire v2.1.0\nSUCCESS\n'

    def test_success_needs_discovery_expected_version_and_unambiguous_success(self):
        validate_au.verify_pluginval_log(self.valid_log(), self.metadata)
        for text in [self.valid_log().replace('found: 1', 'found: 0'),
                     self.valid_log().replace('2.1.0', '2.0.0'),
                     self.valid_log().replace('SUCCESS', 'FAILURE'),
                     self.valid_log() + '!!! Test 1 failed: bad state\n', 'SUCCESS\n']:
            with self.subTest(text=text), self.assertRaises(RuntimeError):
                validate_au.verify_pluginval_log(text, self.metadata)

    def fake_process(self, output, code):
        process = Mock()
        process.stdout = io.StringIO(output)
        process.wait.return_value = process.poll.return_value = code
        return process

    def test_strict_validation_uses_the_registered_identity(self):
        process = self.fake_process(self.valid_log(), 0)
        with patch.object(validate_au.subprocess, 'Popen', return_value=process) as popen, contextlib.redirect_stdout(io.StringIO()):
            validate_au.run_validation(Path('/pluginval'), self.metadata, self.root, 1800000)
        command = popen.call_args.args[0]
        self.assertIn('--strictness-level', command)
        self.assertEqual(command[command.index('--strictness-level') + 1], '10')
        self.assertEqual(command[-1], 'AudioUnit:Effects/aufx,Fire,Wing')
        self.assertNotIn('--disabled-tests', command)
        self.assertEqual((self.root / 'pluginval-au.log').read_text(), self.valid_log())

    def test_validation_failure_is_retained_and_never_retried(self):
        output = 'Num plugins found: 0\n!!! Test 1 failed: No types found.\nFAILURE\n'
        process = self.fake_process(output, 1)
        with patch.object(validate_au.subprocess, 'Popen', return_value=process) as popen, contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(RuntimeError, 'No types found'):
                validate_au.run_validation(Path('/pluginval'), self.metadata, self.root, 1800000)
        self.assertEqual(popen.call_count, 1)
        self.assertEqual((self.root / 'pluginval-au.log').read_text(), output)


if __name__ == '__main__':
    unittest.main()
