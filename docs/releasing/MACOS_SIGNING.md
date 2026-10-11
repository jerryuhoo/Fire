# Optional Developer ID releases

Local builds and CI use complete ad-hoc bundle signatures by default. They do
not require an Apple account. An ad-hoc signature is an integrity signature,
not a Developer ID identity or Apple notarization approval.

A maintainer with a Developer ID Application certificate and an App Store
Connect team API key can enable the existing tag workflow. No credentials are
checked into Fire. Configure these **GitHub Actions repository secrets**:

| Secret | Value |
| --- | --- |
| `FIRE_MACOS_CERTIFICATE_BASE64` | Base64-encoded `.p12` containing the Developer ID Application certificate and private key |
| `FIRE_MACOS_CERTIFICATE_PASSWORD` | Export password for that `.p12` |
| `FIRE_MACOS_SIGNING_IDENTITY` | Full `Developer ID Application: Name (TEAMID)` identity |
| `FIRE_NOTARY_KEY_BASE64` | Base64-encoded App Store Connect `.p8` team API key |
| `FIRE_NOTARY_KEY_ID` | API key ID |
| `FIRE_NOTARY_ISSUER_ID` | API issuer UUID |

Then set the repository **Actions variable** `FIRE_MACOS_NOTARIZE` to `true`.
The step runs only on version tags, after tests and plug-in validation. It
imports the identity into a temporary keychain, signs AU/VST3/CLAP with hardened
runtime and a secure timestamp, recreates the archive and submits it with
`notarytool`. Missing credentials or a result other than `Accepted` fail the
release rather than silently publishing an ad-hoc fallback. Cleanup removes the
temporary certificate, key and keychain even on failure. The submission receipt
is retained as a separate CI artifact, without credentials.

ZIP archives cannot be stapled. This distribution uses Apple's online ticket
lookup for the contained signed plug-ins, so first verification can require an
internet connection. A future installer/DMG can add an offline stapled ticket.
Do not describe a release as notarized unless its successful receipt exists.
Do not re-sign a downloaded Developer ID release locally.

Developer ID credentials are not configured or purchased by building Fire.
The local machine checked during this implementation had no valid signing
identities; the credentialed path therefore requires a real release-run check
when the maintainer provides these secrets. The ordinary ad-hoc path remains
fully usable and tested.

References: [Apple Developer ID](https://developer.apple.com/developer-id/),
[Apple notarization documentation](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution).
