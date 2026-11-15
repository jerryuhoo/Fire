#!/usr/bin/env python3
"""Wait for the installed AU identity/version, then run strict pluginval once."""

import argparse
import json
from pathlib import Path
import plistlib
import re
import subprocess
import sys
import time


def component_metadata(bundle, version):
    match = re.fullmatch(r'(\d+)\.(\d+)\.(\d+)', version)
    if not match:
        raise ValueError('Expected a numeric major.minor.patch AU version')
    major, minor, patch = map(int, match.groups())
    if major > 65535 or minor > 255 or patch > 255:
        raise ValueError('AU version cannot be encoded in its 32-bit version field')
    packed = (major << 16) | (minor << 8) | patch
    with (bundle / 'Contents/Info.plist').open('rb') as source:
        info = plistlib.load(source)
    components = info.get('AudioComponents', [])
    if len(components) != 1:
        raise ValueError('Expected exactly one AudioComponents entry')
    component = components[0]
    identity = [component.get(key, '') for key in ('type', 'subtype', 'manufacturer')]
    if identity != ['aufx', 'Fire', 'Wing']:
        raise ValueError(f'Unexpected Fire AU identity: {identity!r}')
    if (info.get('CFBundleShortVersionString') != version
            or info.get('CFBundleVersion') != version
            or component.get('version') != packed):
        raise ValueError('Installed AU manifest version differs from VERSION')
    if component.get('name') != 'Blue Wings Music: Fire':
        raise ValueError('Unexpected Fire AU component name')
    executable = info.get('CFBundleExecutable', '')
    if not executable or Path(executable).name != executable or not (bundle / 'Contents/MacOS' / executable).is_file():
        raise ValueError('Installed AU executable is missing')
    return {'identity': identity, 'packed_version': packed, 'version': version,
            'name': component['name'], 'identifier': 'AudioUnit:Effects/' + ','.join(identity)}


def wait_for_registration(probe, metadata, log_path, timeout):
    deadline = time.monotonic() + timeout
    attempt = 0
    with log_path.open('w', encoding='utf-8') as log:
        while time.monotonic() < deadline:
            attempt += 1
            command = [str(probe), *metadata['identity'], str(metadata['packed_version'])]
            try:
                # Use a fresh process: AU discovery can be cached per process.
                result = subprocess.run(command, capture_output=True, text=True,
                                        timeout=min(10, max(.1, deadline - time.monotonic())))
                message = result.stdout + result.stderr
                log.write(f'Attempt {attempt}, exit {result.returncode}\n{message}\n')
                log.flush()
                print(message.strip(), flush=True)
                if result.returncode == 0:
                    return
                if result.returncode not in (2, 3):
                    raise RuntimeError(f'AU registry probe failed: {message.strip()}')
            except subprocess.TimeoutExpired:
                log.write(f'Attempt {attempt}: registry query timed out\n')
                log.flush()
            remaining = deadline - time.monotonic()
            if remaining > 0:
                time.sleep(min(2, remaining))
    raise RuntimeError(f'Installed AU identity/version did not appear in the registry within {timeout:g}s; see registry.log')


def verify_pluginval_log(log, metadata):
    expected = re.escape(metadata['name'] + ' v' + metadata['version'])
    if (not re.search(r'(?m)^Num plugins found: 1\s*$', log)
            or not re.search(r'(?m)^' + expected + r'\s*$', log)
            or not re.search(r'(?m)^SUCCESS\s*$', log)
            or re.search(r'(?m)^(?:FAILURE|FAILED!!|\*\*\* FAILED|!!! Test .* failed)', log)):
        raise RuntimeError('AU validation did not confirm one plug-in, the current version and SUCCESS')


def run_validation(validator, metadata, log_dir, timeout_ms):
    path = log_dir / 'pluginval-au.log'
    command = [str(validator), '--strictness-level', '10', '--timeout-ms', str(timeout_ms),
               '--skip-gui-tests', '--verbose', '--validate', metadata['identifier']]
    # Keep all native validation checks. Do not retry a failed DSP/state test.
    with path.open('w', encoding='utf-8') as log:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, encoding='utf-8', errors='replace', bufsize=1)
        try:
            for line in process.stdout:
                log.write(line)
                print(line, end='', flush=True)
            result = process.wait()
        finally:
            process.stdout.close()
            if process.poll() is None:
                process.terminate()
                process.wait()
    if result != 0:
        excerpt = [line for line in path.read_text().splitlines()
                   if re.search(r'No types found|FAILED|FAILURE|!!! Test .* failed|not restored', line)]
        raise RuntimeError(f'pluginval AU failed with exit {result}: ' + ' | '.join(excerpt[:6])[:1500])
    verify_pluginval_log(path.read_text(), metadata)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--pluginval', type=Path, required=True)
    parser.add_argument('--log-dir', type=Path, required=True)
    parser.add_argument('--registration-timeout', type=float, default=60)
    parser.add_argument('--validation-timeout-ms', type=int, default=1800000)
    args = parser.parse_args()
    args.log_dir.mkdir(parents=True, exist_ok=True)
    try:
        if not 0 < args.registration_timeout <= 300 or args.validation_timeout_ms <= 0:
            raise ValueError('Validation timeouts must be positive; registry wait is bounded to 300s')
        metadata = component_metadata(args.bundle, args.version)
        (args.log_dir / 'manifest.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
        wait_for_registration(args.probe.resolve(), metadata, args.log_dir / 'registry.log', args.registration_timeout)
        run_validation(args.pluginval.resolve(), metadata, args.log_dir, args.validation_timeout_ms)
    except (OSError, ValueError, RuntimeError) as error:
        message = str(error).replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
        print(f'::error title=AU validation::{message}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
