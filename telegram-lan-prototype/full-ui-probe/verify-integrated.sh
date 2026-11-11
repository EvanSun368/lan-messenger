#!/bin/sh
# Linux source-lab verification. This is not a Windows/device acceptance test.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake=${CMAKE_BIN:-/tmp/telegram-build-tools/cmake/data/bin/cmake}
ctest=${CTEST_BIN:-/tmp/telegram-build-tools/cmake/data/bin/ctest}
full=${FULL_BUILD_DIR:-/tmp/telegram-full-ui-611-build}
default=${DEFAULT_BUILD_DIR:-/tmp/telegram-app-build}
results=${RESULTS_DIR:-"$here/verification-latest"}
mkdir -p "$results"
rm -f "$results/PASSED"
# Isolate all test data/settings from the interactive lab and user's original app.
runtime=$(mktemp -d /tmp/lanlink-verification.XXXXXX)
trap 'rm -rf "$runtime"' EXIT HUP INT TERM
export HOME="$runtime/home" XDG_CACHE_HOME="$runtime/cache" XDG_DATA_HOME="$runtime/data"
export QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME"
# Build sequentially: concurrent lib_ui/app compilation previously exhausted RAM.
BUILD_DIR="$full" BUILD_TARGET=all BUILD_JOBS=1 "$here/build-local.sh" >"$results/full-build.log" 2>&1
(cd "$full/application" && "$ctest" --output-on-failure) >"$results/full-ctest.log" 2>&1
# Preserve FULL-mode screenshots before default-mode tests overwrite /tmp files.
for view in group-box contact-box transfer-box sharing-box group-identity transfer-live transfer-live-sender sharing-live create-group-narrow contact-profile-narrow; do
 cp "/tmp/original-$view.png" "$results/original-$view.png"
done
cp /tmp/standalone-original-sharing-box-narrow.png "$results/original-sharing-box-narrow.png"
for view in account-configuration-mock window-compose-mock window-inbox-mock; do
 cp "/tmp/mail-$view.png" "$results/mail-$view.png"
done
cp /tmp/mail-chat-draft.png "$results/mail-chat-draft.png"
cp /tmp/mail-window.png "$results/mail-window.png"
cp /tmp/original-settings-box.png "$results/original-settings-box.png"
cp /tmp/original-settings-narrow.png "$results/original-settings-narrow.png"
for view in wide menu narrow; do
 cp "/tmp/original-shell-$view.png" "$results/original-shell-$view.png"
done
cp /tmp/timeline-real-sender.png "$results/telegram-file-timeline-sender.png"
cp /tmp/timeline-real-receiver.png "$results/telegram-file-timeline-receiver.png"
for peer in sender bob carol; do
 cp "/tmp/telegram-group-file-$peer.png" "$results/telegram-group-file-$peer.png"
done
"$cmake" --build "$default" --target all -j1 >"$results/default-build.log" 2>&1
(cd "$default" && "$ctest" --output-on-failure) >"$results/default-ctest.log" 2>&1
for scale in 1.25 1.5 2; do
 QT_SCALE_FACTOR="$scale" "$full/application/test_ui" >"$results/ui-scale-$scale.log" 2>&1
done
printf '%s\n' 'All requested builds and tests passed in this run.' >"$results/PASSED"
