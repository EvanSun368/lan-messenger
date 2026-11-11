# Windows MinGW notes (this fork)

Official Windows instructions in [building-win.md](building-win.md) use
Visual Studio / MSVC, `midl`, and a vendored Libraries tree. This document
covers what can be done with a MinGW cross-compiler on Linux.

This is not an official Telegram Desktop build. Do not present binaries as
official Telegram.

## Winsock port

`lan::Node` talks to the OS through `Telegram/SourceFiles/lan/socket_compat.h`.
On POSIX it uses `sys/socket.h` / `poll` / `fcntl`. On Windows it uses
Winsock 2 (`WSAPoll`, `ioctlsocket`, `closesocket`) and requires
`_WIN32_WINNT >= 0x0600` (the Makefile defines `0x0A00`). Socket handles are
a `Socket` alias, not `int`, because a 64-bit `SOCKET` does not fit in `int`.

`lan_cli.cpp` cannot poll stdin with `WSAPoll` (that API only accepts
sockets). The Windows path uses `PeekNamedPipe` / `WaitForSingleObject`.

## Cross-compile the isolated transport

On a Linux host with `x86_64-w64-mingw32-g++-posix`:

```
cd Telegram/SourceFiles/lan
make CXX=x86_64-w64-mingw32-g++-posix lan_tests lan_node
```

This produces `lan_tests.exe` and `lan_node.exe`, which depend on
`WS2_32.dll` (system) plus the MinGW runtime (`libgcc_s_seh-1.dll`,
`libstdc++-6.dll`, `libwinpthread-1.dll`).

Linux `make test` and `make smoke` should still be run with the host
compiler (the same Makefile, `CXX=g++`). Those tests stay the verification
for the transport; the `.exe` files are not run in the Linux environment
unless Wine is available.

Usage of `lan_node.exe` is the same as the POSIX `lan_node`:

```
lan_node --name Alice --dir alice-data --bind 0.0.0.0
lan_node --name Bob --dir bob-data --bind 0.0.0.0
```

Stdin commands: `list`, `target <ipv4:udp-port>`, `send <peer> <text>`,
`quit`. The loopback smoke test uses `--no-multicast` plus unicast
`--target` because multicast to `127.0.0.1` is unreliable.

## Full tdesktop GUI (not yet)

A packaged MinGW cmake configure of the desktop app was attempted from this
Linux box against Qt 6.8.3 (`mingw_64` + host tools in `gcc_64`, with
`QT_HOST_PATH`, Svg, and ShaderTools/`qsb` added). It does not complete.
Known blockers, in the order they appear or will appear:

- **Qt**: a minimal 6.8.3 tree is not enough. `Qt6Svg` is required, and the
  fork compiles QRhi shaders with `qsb`. Those can be added with aqt
  (`--archives qtsvg`, `--modules qtshadertools`).
- **Boost**: packaged cmake then fails looking for `BoostConfig.cmake` built
  for MinGW. Official tdesktop ships Boost inside its Libraries tree.
- A long list of other libraries also has to exist as MinGW prefixes, not
  Linux ones: OpenSSL, FFmpeg, OpenAL, hunspell, lz4, xxhash, ada, rnnoise,
  tde2e, tg_owt/WebRTC, plus a Windows `rustc` target for `tdesktop_rust`
  (tlottie, wallet-engine).
- **midl**: `Telegram/cmake/generate_midl.cmake` invokes the Microsoft IDL
  compiler for `windows_quiethours.idl` and `windows_toastactivator.idl`.
  There is no `midl` on Linux.
- **MSVC linker flags**: `Telegram/CMakeLists.txt` passes `/PDBPAGESIZE:8192`
  (and similar) under `if (WIN32)`, which GNU ld rejects. Delay-load flags
  are already under `if (MSVC)`.
- Official Windows builds also expect the Windows SDK, WebView2, and
  (for the non-packaged path) NuGet / `prepare` scripts.

Until those are solved, the shippable Windows artifact from a Linux
cross-compile is the `lan_node` / `lan_tests` portable zip, not
`Telegram.exe`.
