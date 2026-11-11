#!/bin/sh
# Pinned source bootstrap; does not overwrite any existing checkout.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST="$ROOT/upstream"
PIN=c4bd064bcc84a359179e191fa6dfd216acda504f
if [ -e "$DEST" ]; then
 echo "Refusing to replace existing $DEST. Verify HEAD is $PIN manually." >&2
 exit 1
fi
git init "$DEST"
git -C "$DEST" remote add origin https://github.com/telegramdesktop/tdesktop.git
git -C "$DEST" fetch --depth=1 origin "$PIN"
git -C "$DEST" checkout --detach FETCH_HEAD
test "$(git -C "$DEST" rev-parse HEAD)" = "$PIN"
git -C "$DEST" submodule update --init --recursive --depth=1 -- \
 cmake Telegram/codegen Telegram/lib_base Telegram/lib_crl \
 Telegram/lib_rpl Telegram/lib_ui Telegram/ThirdParty/GSL \
 Telegram/ThirdParty/expected Telegram/ThirdParty/range-v3 \
 Telegram/ThirdParty/kcoreaddons Telegram/ThirdParty/xdg-desktop-portal \
 Telegram/ThirdParty/lz4 Telegram/ThirdParty/xxHash
if [ ! -e "$ROOT/full-ui-probe/cmake" ]; then
 ln -s ../upstream/cmake "$ROOT/full-ui-probe/cmake"
fi
git -C "$DEST" submodule status --recursive
