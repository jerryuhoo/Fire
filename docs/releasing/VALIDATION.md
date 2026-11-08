# Build and format validation

Configure a separate Release build with `FIRE_INSTALL_PLUGINS=OFF` so validation
does not automatically replace the plug-in currently loaded by a DAW:

```sh
cmake -S . -B Builds/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DFIRE_INSTALL_PLUGINS=OFF
cmake --build Builds/Release --parallel 4
ctest --test-dir Builds/Release --output-on-failure
```

Use the supported formats for the current platform: AU/VST3/CLAP on macOS,
VST3/CLAP on Windows and Linux. macOS distribution builds use
`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`. The smoke host uses APIs compatible
with the plug-in's minimum deployment target.

## Tests in CI

Windows uses the hosted runner's preinstalled LLVM, and macOS uses the newest
stable Xcode on its image. Windows does not upgrade LLVM through Chocolatey
during job setup. The logs print the actual compiler version for diagnosis.
Ninja is installed only if it is absent from the Windows/macOS image.

Compiler caching is optional: CI uses an existing `sccache` only when its server
starts successfully. Otherwise CMake's compiler launchers are explicitly empty
and the same build and tests run without caching. CI does not download sccache
or run its former post-job action. An unavailable cache can increase build time,
but must not prevent validation. The required, version-pinned pluginval download
has bounded HTTP retries. If the normal release URL fails, it resolves the exact
tag and platform asset through GitHub's official release asset API. If both
download routes fail, CI builds pluginval from the pinned v1.0.4 source commit
`ed19c2c16b57a6d94db391bea3ef4a80b769d5bf` and its recorded JUCE submodule,
then runs the validator's own tests. It does not substitute Fire's JUCE or skip
any AU/VST3 checks. This setup runs before Fire's compilation. A source/version
mismatch, build failure or failed validator self-test still fails the job;
failed Fire validation never triggers this setup fallback.

The source fallback leaves pluginval's `TestUtilities.cpp` unoptimised so recent
compilers cannot remove the unused-vector allocations that its allocation
self-tests deliberately check. This is a per-source CMake build option, not an
upstream source patch; the rest of pluginval and all Fire targets remain Release
builds. The validator's complete self-test category must pass before use.

The normal CTest run covers DSP, state, UI regressions and benchmarks, including:

- Stereo spectrum selection, opposite-phase/right-only audio and fresh capture
  after a display is hidden or shown.
- Macro names in host/preset/A-B state and undo history; macro editor lifetime
  and stale rename rejection.
- Browser filters, scroll/session retention, recent sounds, target search,
  stable parameter mapping and control navigation.
- Settings diagnostics, appearance and control layout.
- **CLAP load, audio and state roundtrip**: opens the distributed binary through
  CLAP's public host API, changes a parameter, saves via partial stream writes,
  restores into a fresh instance via partial reads, and compares all exposed
  parameter values. It processes non-silent stereo audio at 44.1/48/96 kHz and
  64/512 samples, rejecting non-finite output. The expected plug-in version must
  match `VERSION`.

To run just the CLAP binary check after building:

```sh
ctest --test-dir Builds/Release --output-on-failure -R '^CLAP load, audio and state roundtrip$'
```

The helper is built as `FireClapSmoke` under the build's `validation` directory;
multi-configuration generators may place it in a configuration subdirectory.
It does not link Fire's processing implementation, so it exercises the actual
format wrapper rather than only the shared code.

## CLAP GUI scaling on Windows

The Windows CI also runs `CLAP Windows GUI DPI and resize contract` against
the built CLAP binary. It embeds the editor in a native Win32 parent and checks
100%, 125%, 150% and 200% host scales, minimum/adjusted sizes, live scale changes,
resizing and reopening. The native child client size must match CLAP's reported
physical pixel size, including when the host accepts `request_resize` without
calling `set_size` back into the plug-in.

```sh
ctest --test-dir Builds/Release --output-on-failure -R '^CLAP Windows GUI DPI and resize contract$'
```

A separate Windows unit regression applies controlled 125%, 150% and 200%
native peer scales, so duplicate DPI application is caught even on a 100% CI
desktop. The generated CLAP wrapper pins its peer to 1:1 after attachment and
refreshes the native bounds; the editor already applies the host scale. Cocoa
retains its logical-point convention. This backports the mechanism from
[clap-juce-extensions #183](https://github.com/free-audio/clap-juce-extensions/pull/183)
for [Fire #101](https://github.com/jerryuhoo/Fire/issues/101), without modifying
the pinned submodule. Before release, also check Bitwig on Windows at 125%:
open the editor, resize it, reopen it and move it between differently scaled
displays. A macOS build cannot establish that native Windows/DAW result.

## AU and VST3 validation

CI pins [Tracktion pluginval](https://github.com/Tracktion/pluginval) to v1.0.4,
uses strictness 10 and permits 30 minutes for exhaustive parameter/state checks.
VST3 runs on every platform, and AU additionally runs on macOS. macOS skips
pluginval's GUI tests; Fire's native GUI regression suite runs separately in
CTest. AU discovery uses macOS's component registry: passing a new `.component`
path alone does not guarantee that version is loaded. CI copies the just-built
AU into the disposable runner's user Components folder, verifies its signature,
touches the Components directory and refreshes that runner user's
AudioComponentRegistrar. `AuRegistryProbe` then checks the exact
`aufx / Fire / Wing` identity and packed version in fresh processes, with a
bounded 60-second registration wait. A bundle on disk is not proof that macOS
has published its registry entry.

Once registered, CI runs pluginval at strictness 10 against
`AudioUnit:Effects/aufx,Fire,Wing`. It requires a zero exit status, exactly one
discovered plug-in, the expected version and `SUCCESS`. Only missing or stale
registry entries are retried; a failed validation is never retried or ignored.
The `au-validation-macOS` artifact retains the manifest, signature, registrar
refresh, registry probe and pluginval logs even if this step fails.
For a local AU check, close audio hosts, preserve any existing
installed Fire bundle, install the build you intend to test and verify its
version in the validator output. Do not overwrite a running host's plug-in.
VST3 loads directly from the supplied build path. Run the two validators
serially when benchmarking on the same machine:

```sh
pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --timeout-ms 1800000 --skip-gui-tests --verbose --validate Builds/Release/Fire_artefacts/Release/VST3/Fire.vst3
pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --timeout-ms 1800000 --skip-gui-tests --verbose --validate 'AudioUnit:Effects/aufx,Fire,Wing'
```

For version 2.1.0, confirm that the AU log contains both
`Blue Wings Music: Fire v2.1.0` and `SUCCESS`, and that the command exits with
zero status. Use the version from `VERSION` when checking later builds. An
installed bundle's Info.plist alone does not verify what the AU registry loaded.

The CI readiness helper can be tested without changing any local AU installation:

```sh
python3 -m unittest discover -s tools/validation -p 'test_validate_au.py' -v
xcrun clang++ -std=c++17 tools/validation/AuRegistryProbe.cpp -framework AudioToolbox -framework CoreFoundation -o /tmp/fire-au-registry-probe
/tmp/fire-au-registry-probe aufx Fire Wing 131328
```

The last command only queries the registry. Exit 0 means version 2.1.0 is
registered, 2 means absent, 3 means another version, and 1 indicates an error.
The registrar refresh in the workflow is for disposable CI runners, not a step
to run while a user is working in a DAW.

Use your installed/downloaded pluginval path. On Windows/Linux use
`pluginval.exe`/`pluginval`, the VST3 artifact path, and omit `--skip-gui-tests`
when a display is available (CI supplies Xvfb on Linux).

macOS packaging also verifies each bundle:

```sh
codesign --verify --deep --strict Builds/Release/Fire_artefacts/Release/AU/Fire.component
codesign --verify --deep --strict Builds/Release/Fire_artefacts/Release/VST3/Fire.vst3
codesign --verify --deep --strict Builds/Release/Fire_artefacts/Release/CLAP/Fire.clap
```

A valid ad-hoc signature does not imply Apple notarization. See the
[optional Developer ID workflow](MACOS_SIGNING.md). A configured notarization
failure blocks tag publication; an unconfigured workflow retains ad-hoc builds.

## Performance reports and support

The repeatable `[cpu-realtime]` scenarios compare one/four serial instances,
48/96 kHz, 64/128-sample buffers, default and heavy analogue HQ patches, with
display capture enabled/disabled. `[cpu-spectrum]` measures capture plus FFT for
Left/Right/Stereo modes. These reports describe the test machine and build;
they are not a promise about another host's CPU meter. Run performance probes
without other validators or builds competing for CPU. Commands and optional
extended scenarios are in [CPU_AUDIT.md](../../benchmarks/CPU_AUDIT.md).

For a reproducible bug report, paste **Menu → Settings → Copy diagnostics**,
state the host operation/preset, and attach a minimal example if appropriate.
The report is generated locally. Automated tests and format validators do not
replace checking the published packages in the intended DAWs.
