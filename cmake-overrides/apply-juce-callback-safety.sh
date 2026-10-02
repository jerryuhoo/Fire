#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
juce_dir="$root/JUCE"
patch="$root/cmake-overrides/juce-9.0.2-callback-safety.patch"
expected=72782788ce18c2d4d760b28e0921d6ffc6431102
if [ "$(git -C "$juce_dir" rev-parse HEAD)" != "$expected" ]; then
    echo "Fire's JUCE safety patch requires the pinned JUCE revision; initialise submodules or review the patch before upgrading." >&2
    exit 1
fi
if git -C "$juce_dir" apply --reverse --check "$patch" >/dev/null 2>&1; then
    exit 0
fi
git -C "$juce_dir" apply --check "$patch"
git -C "$juce_dir" apply "$patch"
