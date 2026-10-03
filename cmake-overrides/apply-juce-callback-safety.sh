#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
juce_dir="$root/JUCE"
patch="$root/cmake-overrides/juce-9.0.3-callback-safety.patch"
expected=be29c81492b6151c8ea8d14c840e1311963b3a83
if [ "$(git -C "$juce_dir" rev-parse HEAD)" != "$expected" ]; then
    echo "Fire's JUCE safety patch requires the pinned JUCE revision; initialise submodules or review the patch before upgrading." >&2
    exit 1
fi
if git -C "$juce_dir" apply --reverse --check "$patch" >/dev/null 2>&1; then
    exit 0
fi
git -C "$juce_dir" apply --check "$patch"
git -C "$juce_dir" apply "$patch"
