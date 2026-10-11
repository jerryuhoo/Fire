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

## AU and VST3 validation

CI pins [Tracktion pluginval](https://github.com/Tracktion/pluginval) to v1.0.4,
uses strictness 10 and permits 30 minutes for exhaustive parameter/state checks.
VST3 runs on every platform, and AU additionally runs on macOS. macOS skips
pluginval's GUI tests; Fire's native GUI regression suite runs separately in
CTest. AU discovery uses macOS's component registry: passing a new `.component`
path alone does not guarantee that version is loaded. CI copies the just-built
AU into the disposable runner's user Components folder before launching the
validator, then requires its log to report both the expected version and success.
For a local AU check, close audio hosts, preserve any existing
installed Fire bundle, install the build you intend to test and verify its
version in the validator output. Do not overwrite a running host's plug-in.
VST3 loads directly from the supplied build path. Run the two validators
serially when benchmarking on the same machine:

```sh
pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --timeout-ms 1800000 --skip-gui-tests --verbose --validate Builds/Release/Fire_artefacts/Release/VST3/Fire.vst3
pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --timeout-ms 1800000 --skip-gui-tests --verbose --validate "$HOME/Library/Audio/Plug-Ins/Components/Fire.component"
```

For version 2.0.1, confirm that the AU log contains both
`Blue Wings Music: Fire v2.0.1` and `SUCCESS`, and that the command exits with
zero status. Use the version from `VERSION` when checking later builds. An
installed bundle's Info.plist alone does not verify what the AU registry loaded.

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
