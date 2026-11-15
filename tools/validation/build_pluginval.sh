#!/usr/bin/env bash
# Last-resort validator setup; never used to retry a failed plug-in validation.
set -euo pipefail
platform="${1:?Expected Linux, macOS or Windows}"
case "$platform" in Linux|macOS|Windows) ;; *) echo "Unsupported platform: $platform" >&2; exit 1 ;; esac
: "${PLUGINVAL_VERSION:?Pinned pluginval version is required}"
: "${PLUGINVAL_SOURCE_REV:?Pinned pluginval commit is required}"
: "${RUNNER_TEMP:?Temporary directory is required}"

source_dir="$RUNNER_TEMP/fire-pluginval-source"
build_dir="$RUNNER_TEMP/fire-pluginval-build"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
git clone --depth 1 --branch "$PLUGINVAL_VERSION" --recurse-submodules --shallow-submodules \
  https://github.com/Tracktion/pluginval.git "$source_dir"
actual_revision="$(git -C "$source_dir" rev-parse HEAD)"
actual_version="$(tr -d '\r\n' < "$source_dir/VERSION")"
if [[ "$actual_revision" != "$PLUGINVAL_SOURCE_REV" || "v$actual_version" != "$PLUGINVAL_VERSION" ]]; then
  echo "pluginval source does not match the pinned commit and version" >&2
  exit 1
fi
echo "Building pluginval $actual_version from $actual_revision"
git -C "$source_dir" submodule status --recursive

# Fire validates AU/VST3. No proprietary VST2 SDK is needed for this fallback.
unset VST2_SDK_DIR
cmake -S "$source_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER_LAUNCHER= -DCMAKE_CXX_COMPILER_LAUNCHER= \
  -DCMAKE_PROJECT_pluginval_INCLUDE="$script_dir/PluginvalBuild.cmake"
cmake --build "$build_dir" --config Release --target pluginval --parallel 2
artefacts="$build_dir/pluginval_artefacts/Release"
case "$platform" in
  macOS)
    # A failed archive extraction may have left an incomplete bundle here.
    cmake -E rm -rf pluginval.app
    cmake -E copy_directory "$artefacts/pluginval.app" pluginval.app
    validator=pluginval.app/Contents/MacOS/pluginval
    ;;
  Windows)
    cmake -E copy "$artefacts/pluginval.exe" pluginval.exe
    validator=./pluginval.exe
    ;;
  Linux)
    cmake -E copy "$artefacts/pluginval" pluginval
    validator=./pluginval
    ;;
esac
"$validator" --run-tests
