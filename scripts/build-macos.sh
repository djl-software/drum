#!/bin/bash
set -euo pipefail
if [[ "$(uname -s)" != Darwin ]]; then
    echo 'This script requires macOS and Xcode command-line tools.' >&2
    exit 1
fi
root="$(cd "$(dirname "$0")/.." && pwd)"
build="${1:-$root/build-macos}"
args=(-S "$root" -B "$build" -G Xcode
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
    "-DDRUM_BUILD_STANDALONE=${DRUM_BUILD_STANDALONE:-OFF}")
if [[ -n "${JUCE_SOURCE_DIR:-}" ]]; then args+=("-DJUCE_SOURCE_DIR=$JUCE_SOURCE_DIR"); fi
cmake "${args[@]}"
cmake --build "$build" --config Release --parallel "${DRUM_BUILD_JOBS:-4}"
ctest --test-dir "$build" -C Release --output-on-failure
echo "VST3 development bundle: $build/DJLDrum_artefacts/Release/VST3"
echo 'These are development builds. Distribution requires Developer ID signing, notarization and runtime validation.'
