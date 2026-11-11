#!/bin/sh
set -eu
export GI_GIR_PATH=/tmp/telegram-sysroot/usr/share/gir-1.0:/tmp/telegram-sysroot/usr/lib/x86_64-linux-gnu/gir-1.0
export XDG_DATA_DIRS=/tmp/telegram-sysroot/usr/share:/tmp/telegram-sysroot/usr/lib/x86_64-linux-gnu
export PKG_CONFIG_PATH=/tmp/telegram-sysroot/usr/lib/x86_64-linux-gnu/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/tmp/telegram-sysroot
export PATH=/tmp/telegram-sysroot/usr/bin:$PATH
export PYTHONPATH=/tmp/telegram-gir-python:/tmp/telegram-sysroot/usr/share/glib-2.0:/tmp/telegram-sysroot/usr/lib/x86_64-linux-gnu/gobject-introspection:/tmp/telegram-sysroot/usr/lib/python3/dist-packages
exec /tmp/telegram-build-tools/cmake/data/bin/cmake --build "${BUILD_DIR:-/tmp/telegram-full-ui-611-build}" --target "${BUILD_TARGET:-input_field_probe}" -j"${BUILD_JOBS:-2}"
