#!/bin/sh
set -eu
QT_ROOT=${QT_ROOT:-/tmp/telegram-qt-new/6.11.2/gcc_64}
BUILD_DIR=${BUILD_DIR:-/tmp/telegram-full-ui-611-build}
export PKG_CONFIG_PATH=/tmp/telegram-sysroot/usr/lib/x86_64-linux-gnu/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/tmp/telegram-sysroot
export PATH=/tmp/telegram-sysroot/usr/bin:$PATH
exec /tmp/telegram-build-tools/cmake/data/bin/cmake -S "$(dirname "$0")" -B "$BUILD_DIR" \
 -DCMAKE_PREFIX_PATH="$QT_ROOT;/tmp/telegram-sysroot/usr" \
 -DOPENGL_INCLUDE_DIR=/tmp/telegram-sysroot/usr/include \
 -DOPENGL_gl_LIBRARY=/usr/lib/x86_64-linux-gnu/libGL.so.1 \
 -DOpenGL_GL_PREFERENCE=LEGACY \
 -DXKB_LIBRARY=/usr/lib/x86_64-linux-gnu/libxkbcommon.so.0 \
 -DXKB_INCLUDE_DIR=/tmp/telegram-sysroot/usr/include \
 -DBOOST_ROOT=/tmp/telegram-sysroot/usr \
 -DGIR_DIR=/tmp/telegram-sysroot/usr/share/gir-1.0 \
 -DDESKTOP_APP_GIRSCANNER=/tmp/telegram-gir-scanner "$@"
