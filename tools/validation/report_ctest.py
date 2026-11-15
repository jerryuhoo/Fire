#!/usr/bin/env python3
"""Publish bounded CTest diagnostics; the preceding CTest step owns pass/fail."""

import argparse
import html
import os
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

REPORTS = ('test-layout-results.xml', 'test-results.xml')
MAX_FAILURES = 20
MAX_DETAIL = 3000


def annotation(level, title, message):
    # A test name or assertion must never become another workflow command.
    escaped = message.replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
    print(f'::{level} title={title}::{escaped}')


def failure_detail(case):
    chunks = []
    for tag in ('failure', 'error', 'system-out', 'system-err'):
        for element in case.findall(tag):
            chunks.extend([element.get('message', ''), element.text or ''])
    lines = '\n'.join(chunks).strip().splitlines()
    # Catch2 puts assertion details in system-out, often after a long banner.
    selected = set()
    for index, line in enumerate(lines):
        if re.search(r'FAILED:|fatal error|exception|timed out|timeout', line, re.IGNORECASE):
            selected.update(range(max(0, index - 1), min(len(lines), index + 8)))
    detail = '\n'.join(lines[i] for i in sorted(selected)) if selected else '\n'.join(lines[-25:])
    if not detail:
        detail = 'CTest marked this case as failed without assertion output. See LastTest.log.'
    return detail[:MAX_DETAIL] + ('\n[excerpt truncated; see diagnostic artifact]' if len(detail) > MAX_DETAIL else '')


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--summary', type=Path, default=os.environ.get('GITHUB_STEP_SUMMARY'))
    args = parser.parse_args()
    summary = ['## CTest results', '']
    failures = []
    total = skipped = 0
    readable = 0
    for filename in REPORTS:
        path = args.build_dir / filename
        if not path.exists():
            summary.append(f'- `{filename}` was not produced; the test step may have stopped before this phase.')
            continue
        try:
            cases = list(ET.parse(path).getroot().iter('testcase'))
        except (OSError, ET.ParseError) as error:
            annotation('warning', 'CTest report unavailable', f'{filename}: {error}')
            summary.append(f'- `{filename}` could not be read; inspect the diagnostic artifact.')
            continue
        readable += 1
        phase_failures = []
        phase_skipped = 0
        for case in cases:
            if (case.find('failure') is not None or case.find('error') is not None
                    or case.get('status') == 'fail'):
                phase_failures.append((case.get('name', '(unnamed test)'), failure_detail(case)))
            elif case.find('skipped') is not None or case.get('status') in ('notrun', 'disabled', 'skipped'):
                phase_skipped += 1
        failures.extend(phase_failures)
        total += len(cases)
        skipped += phase_skipped
        summary.append(f'- `{filename}`: {len(cases)} tests, {len(phase_failures)} failed, {phase_skipped} skipped.')
    if not readable:
        annotation('warning', 'CTest report unavailable', 'No readable JUnit report was produced. Inspect LastTest.log and the test step output.')
        summary.extend(['', 'No readable JUnit report was produced. This does not establish that tests passed.'])
    else:
        print(f'CTest reports: {total} tests, {len(failures)} failed, {skipped} skipped.')
    for name, detail in failures[:MAX_FAILURES]:
        annotation('error', 'CTest failure', f'{name}\n{detail}')
        summary.extend(['', f'<details><summary>{html.escape(name)}</summary>', '',
                        f'<pre>{html.escape(detail)}</pre>', '</details>'])
    if len(failures) > MAX_FAILURES:
        summary.extend(['', f'{len(failures) - MAX_FAILURES} additional failures are in the diagnostic artifact.'])
    summary.extend(['', 'The complete JUnit reports and CTest logs are retained in the platform test-results artifact.', ''])
    if args.summary:
        with args.summary.open('a', encoding='utf-8') as output:
            output.write('\n'.join(summary))
    # Reporting must not replace or suppress the original CTest exit status.
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
