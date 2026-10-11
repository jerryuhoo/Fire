#!/bin/bash
# Opt-in release step. Never run for pull requests; the default build remains
# ad-hoc signed and does not require Apple credentials.
set -euo pipefail
set +x
umask 077

if [[ $# != 2 ]]; then
    echo 'Usage: sign-and-notarize-macos.sh <artefacts-directory> <output.zip>' >&2
    exit 2
fi
for required in FIRE_MACOS_CERTIFICATE_BASE64 FIRE_MACOS_CERTIFICATE_PASSWORD FIRE_MACOS_SIGNING_IDENTITY FIRE_NOTARY_KEY_BASE64 FIRE_NOTARY_KEY_ID FIRE_NOTARY_ISSUER_ID; do
    if [[ -z "${!required:-}" ]]; then
        echo "Notarization was enabled but $required is not configured." >&2
        exit 1
    fi
done
[[ "$FIRE_MACOS_SIGNING_IDENTITY" == 'Developer ID Application:'* ]] || {
    echo 'Use a Developer ID Application signing identity.' >&2; exit 1;
}

artifacts="$(cd "$1" && pwd)"
archive="$2"
work="$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/fire-notary.XXXXXX")"
keychain="$work/signing.keychain-db"
cleanup() {
    security delete-keychain "$keychain" >/dev/null 2>&1 || true
    rm -rf "$work"
}
trap cleanup EXIT
# The keychain exists only for this step and never becomes the user's default.
keychain_password="$(openssl rand -hex 32)"
printf '%s' "$FIRE_MACOS_CERTIFICATE_BASE64" | base64 --decode > "$work/certificate.p12"
printf '%s' "$FIRE_NOTARY_KEY_BASE64" | base64 --decode > "$work/AuthKey.p8"
security create-keychain -p "$keychain_password" "$keychain"
security set-keychain-settings -lut 21600 "$keychain"
security unlock-keychain -p "$keychain_password" "$keychain"
security import "$work/certificate.p12" -k "$keychain" -P "$FIRE_MACOS_CERTIFICATE_PASSWORD" -T /usr/bin/codesign >/dev/null
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$keychain_password" "$keychain" >/dev/null

for relative in 'AU/Fire.component' 'VST3/Fire.vst3' 'CLAP/Fire.clap'; do
    bundle="$artifacts/$relative"
    [[ -d "$bundle" ]] || { echo "Missing bundle: $relative" >&2; exit 1; }
    codesign --force --options runtime --timestamp --keychain "$keychain" \
        --sign "$FIRE_MACOS_SIGNING_IDENTITY" "$bundle"
    codesign --verify --deep --strict "$bundle"
    mkdir -p "$work/package/$(dirname "$relative")"
    ditto "$bundle" "$work/package/$relative"
done
# Zip files cannot carry stapled tickets. Apple's online Gatekeeper lookup
# retrieves the notarization record for these signed plug-in bundles.
rm -f "$archive"
ditto -c -k --sequesterRsrc "$work/package" "$archive"
xcrun notarytool submit "$archive" --key "$work/AuthKey.p8" \
    --key-id "$FIRE_NOTARY_KEY_ID" --issuer "$FIRE_NOTARY_ISSUER_ID" \
    --wait --timeout 30m --output-format json > "$work/result.json"
python3 - "$work/result.json" <<'PY'
import json, sys
result = json.load(open(sys.argv[1]))
if result.get('status') != 'Accepted':
    raise SystemExit('Apple notarization did not accept this release: ' + str(result.get('status')))
print('Developer ID signing and Apple notarization accepted. Submission: ' + result['id'])
PY
# Retain the non-secret receipt with CI artifacts for release auditing.
cp "$work/result.json" "$artifacts/notarization-receipt.json"
