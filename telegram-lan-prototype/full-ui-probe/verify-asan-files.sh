#!/bin/sh
# Instrument LanLink integration sources (not the prebuilt Qt libraries).
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake=${CMAKE_BIN:-/tmp/telegram-build-tools/cmake/data/bin/cmake}
build=${ASAN_BUILD_DIR:-/tmp/telegram-asan-files-build}
qt=${QT_ROOT:-/tmp/telegram-qt-new/6.11.2/gcc_64}
results=${RESULTS_DIR:-"$here/verification-asan"}
mkdir -p "$results"
rm -f "$results/PASSED"
"$cmake" -S "$here/../application" -B "$build" \
 -DCMAKE_PREFIX_PATH="$qt" \
 -DCMAKE_BUILD_TYPE=Debug -DLANLINK_FULL_TELEGRAM_UI=OFF \
 -DOPENGL_INCLUDE_DIR=/tmp/telegram-sysroot/usr/include \
 -DOPENGL_gl_LIBRARY=/usr/lib/x86_64-linux-gnu/libGL.so.1 \
 '-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer' \
 '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address' >"$results/configure.log" 2>&1
"$cmake" --build "$build" --target test_files -j1 >"$results/build.log" 2>&1
runtime=$(mktemp -d /tmp/lanlink-asan.XXXXXX)
trap 'rm -rf "$runtime"' EXIT HUP INT TERM
mkdir -p "$runtime/home" "$runtime/cache" "$runtime/data"
HOME="$runtime/home" XDG_CACHE_HOME="$runtime/cache" XDG_DATA_HOME="$runtime/data" \
 QT_QPA_PLATFORM=offscreen ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
 "$build/test_files" >"$results/test-files.log" 2>&1
printf '%s\n' 'Instrumented test_files passed. Prebuilt Qt uninstrumented; leak checks disabled.' >"$results/PASSED"
