#!/usr/bin/env bash
#
# Build the Simple Delay plugin.
#
#   ./build.sh          build the plugin (Release)
#   ./build.sh --test   build, then run both offline test suites
#
# Finds CMake on PATH, falling back to the copy bundled with the Visual Studio
# Build Tools. Configures the build tree on first run. Does not clone JUCE --
# if it is missing, the clone command is printed.

set -euo pipefail

cd "$(dirname "$0")"

usage() {
    sed -n '3,8p' "$0" | sed 's/^# \{0,1\}//'
}

CONFIG=Release
RUN_TESTS=0

for arg in "$@"; do
    case "$arg" in
        --test|-t) RUN_TESTS=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "error: unknown option: $arg" >&2; usage >&2; exit 2 ;;
    esac
done

find_cmake() {
    if command -v cmake >/dev/null 2>&1; then
        command -v cmake
        return 0
    fi
    local bundled="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    if [ -x "$bundled" ]; then
        echo "$bundled"
        return 0
    fi
    return 1
}

if ! CMAKE=$(find_cmake); then
    echo "error: cmake not found." >&2
    echo "  Install it with:  winget install Kitware.CMake   (then open a new shell)" >&2
    echo "  or install the Visual Studio Build Tools with the C++ workload." >&2
    exit 1
fi

if [ ! -f libs/JUCE/CMakeLists.txt ]; then
    cat >&2 <<'EOF'
error: libs/JUCE is missing. Clone it once with:

  git clone --depth 1 --branch 8.0.15 --recurse-submodules --shallow-submodules \
      https://github.com/juce-framework/JUCE.git libs/JUCE
EOF
    exit 1
fi

if [ ! -f build/CMakeCache.txt ]; then
    echo "==> configuring build/"
    "$CMAKE" -S . -B build -G "Visual Studio 17 2022" -A x64
fi

echo "==> building (${CONFIG})"
"$CMAKE" --build build --config "$CONFIG" --parallel

if [ "$RUN_TESTS" -eq 1 ]; then
    echo "==> running offline tests"
    ./build/Tests/DelayLineTests.exe
    ./build/Tests/ProcessorTests.exe
fi

echo "==> done"
echo "    VST3:       build/SimpleDelay_artefacts/${CONFIG}/VST3/Simple Delay.vst3"
echo "    Standalone: build/SimpleDelay_artefacts/${CONFIG}/Standalone/Simple Delay.exe"
