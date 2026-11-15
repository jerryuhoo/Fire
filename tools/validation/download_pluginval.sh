#!/usr/bin/env bash
# Download the pinned official validator; failures are handled by the CI caller.
set -euo pipefail
platform="${1:?Expected Linux, macOS or Windows}"
case "$platform" in Linux|macOS|Windows) ;; *) echo "Unsupported platform: $platform" >&2; exit 1 ;; esac
: "${PLUGINVAL_VERSION:?Pinned pluginval version is required}"
: "${RUNNER_TEMP:?Temporary directory is required}"

archive="pluginval_$platform.zip"
download_options=(--fail --location --retry 2 --retry-delay 5 --retry-max-time 180 --connect-timeout 15 --max-time 90)
if ! curl "${download_options[@]}" --output "$archive.part" \
    "https://github.com/Tracktion/pluginval/releases/download/$PLUGINVAL_VERSION/$archive"; then
  echo "::warning::Release download failed; trying GitHub's official release asset API."
  release_json="$RUNNER_TEMP/pluginval-release.json"
  curl "${download_options[@]}" --output "$release_json" \
    "https://api.github.com/repos/Tracktion/pluginval/releases/tags/$PLUGINVAL_VERSION"
  asset_id="$(python3 - "$release_json" "$PLUGINVAL_VERSION" "$archive" <<'PY'
import json, sys
with open(sys.argv[1], encoding='utf-8') as source:
    release = json.load(source)
assets = [a for a in release.get('assets', []) if a.get('name') == sys.argv[3]]
if release.get('tag_name') != sys.argv[2] or len(assets) != 1:
    raise SystemExit('Pinned pluginval release/asset not found')
asset_id = assets[0].get('id')
if type(asset_id) is not int or asset_id <= 0:
    raise SystemExit('Invalid pluginval asset ID')
print(asset_id)
PY
  )"
  curl "${download_options[@]}" -H 'Accept: application/octet-stream' \
    --output "$archive.part" \
    "https://api.github.com/repos/Tracktion/pluginval/releases/assets/$asset_id"
fi
mv "$archive.part" "$archive"
7z x "$archive"
