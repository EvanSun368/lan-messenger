# Packaged Linux desktop build (this fork)

Official Linux instructions in [building-linux.md](building-linux.md) use
Docker / `centos_env` and a vendored Qt 6.11 toolchain. This document is for
a **packaged** build against **system Qt**, verified on Debian 13 (trixie)
with Qt 6.8.2 and GCC 14, without Docker.

This is not an official Telegram Desktop build. Do not present the binary as
official Telegram. `LANLINK_SERVERLESS` defaults ON, and a stock binary of
this fork does not open sockets to official Telegram DCs.

## Checkout

```
git clone --recursive https://github.com/EvanSun368/lan-messenger.git
cd lan-messenger
# or, in an existing tree:
git submodule update --init --recursive
```

The nested QuickJS submodule under
`Telegram/ThirdParty/widget-sandbox/packages/ios/Sources/CQuickJS/quickjs`
must be present.

Apply the packaged-Qt / GCC 14 patches to the `lib_ui` and `cmake`
submodules (these stay as patches; submodule remotes are not retargeted):

```
./Telegram/build/apply-packaged-linux-patches.sh
```

## Extra libraries

`DESKTOP_APP_USE_PACKAGED` defaults ON when `../Libraries` is absent. A
Debian Qt 6.8 install still needs a few libraries that are missing or too
old in the distro. Build them into a prefix (for example `$HOME/lanlink-deps`)
and pass that prefix as `CMAKE_PREFIX_PATH`. Versions that worked in this
environment:

- [ada-url](https://github.com/ada-url/ada) v3.3.0 (official docker uses v3.2.9).
- [rnnoise](https://github.com/desktop-app/rnnoise) (desktop-app fork) plus a
  `rnnoise.pc` in the prefix so pkg-config can find it.
- tde2e: [tdlib/td](https://github.com/tdlib/td) at `51743dfd` configured with
  `-DTD_E2E_ONLY=ON`.
- [tg_owt](https://github.com/desktop-app/tg_owt) at `020feec5`, configured
  with `-DCMAKE_DISABLE_FIND_PACKAGE_absl=TRUE`. Debian's abseil
  (`absl::debian7`) is C++17 and fails `std::string = string_view`.
- `tdesktop_rust`: Rust **1.96.1** (Debian's rustc 1.85 is too old for
  wallet-engine's edition 2024), [tlottie](https://github.com/desktop-app/tlottie)
  `92df98dc`, [wallet-engine](https://github.com/desktop-app/wallet-engine)
  `e59e0d89`, and `wallet-engine.patch` from [desktop-app/patches](https://github.com/desktop-app/patches)
  `e47f866`. Install the resulting `libtdesktop_rust` and headers into the
  same prefix.

System Qt Shader Baker may not be on `PATH`. On Debian it lives at
`/usr/lib/qt6/bin/qsb`. Pass `-DQSB_EXECUTABLE=...` or rely on the extra
hints in `Telegram/cmake/qrhi_shaders.cmake`.

Debian 13 moved `GLib-2.0.gir` under the multiarch path. cppgir needs:

```
export GI_GIR_PATH=/usr/lib/x86_64-linux-gnu/gir-1.0:/usr/share/gir-1.0
```

Install the Qt WebP image plugin (`qt6-image-formats-plugins` on Debian)
**before running** the app. Emoji sprites are `.webp`; without the plugin the
process aborts in `emoji_config.cpp`.

## Configure and build

```
cmake -S Telegram -B /path/to/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/deps \
  -DQSB_EXECUTABLE=/usr/lib/qt6/bin/qsb \
  -DDESKTOP_APP_DISABLE_CRASH_REPORTS=ON \
  -DDESKTOP_APP_DISABLE_AUTOUPDATE=ON

cmake --build /path/to/build -j$(nproc) --target Telegram
```

`wallet/wallet_content.cpp` can use several gigabytes of RAM. On a ~16 GiB
host, a high `-j` may OOM-kill `cc1plus`. If that happens, compile that
translation unit with `-j1` and resume the rest of the build.

The desktop binary is written to `/path/to/build/bin/Telegram`. The
`OUTPUT_NAME` is still `Telegram` (leftover official target name). Crash
reports and autoupdate are disabled in the flags above.

## Run

Use an isolated work directory. Official tdesktop ignores `--help` and may
still launch, writing into `~/.local/share/TelegramDesktop/` if you do not
pass `-workdir`.

```
./bin/Telegram -noupdate -workdir /tmp/lanlink-a
```

The window title is **LanLink**. The intro is a local display-name form; it
is not a phone or QR login. A second instance with a different `-workdir`
binds the same UDP discovery port and can form a direct TCP session on the
LAN.

## Known leftovers after the first packaged run

- The dialogs list can stay on "Loading..." even after two instances have an
  established LAN TCP session. Peer names may not appear in the UI yet
  (`userIsContactChanged() called for a not loaded user!` is logged). Isolated
  `lan_tests` / `tests/smoke_two_processes.sh` still cover the transport.
- The intro still labels the field "First name" and keeps official paper-plane
  art.
- App ID `org.telegram.desktop` and window class `TelegramDesktop` are
  unchanged; the taskbar may still say TelegramDesktop.
- `lan::Node` has a Winsock port. Cross-compiling the full Windows GUI is
  documented, including current blockers, in
  [building-windows-mingw.md](building-windows-mingw.md).
