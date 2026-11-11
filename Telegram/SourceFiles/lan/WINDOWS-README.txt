LanLink LAN transport for Windows (x64)
=======================================

This zip is a MinGW-cross-compiled portable build of the LanLink LAN
transport (lan_node) and its unit tests. It is NOT the full LanLink
desktop GUI, and it is not official Telegram.

What is included
----------------
- lan_node.exe     LAN discovery + direct TCP text chat CLI
- lan_tests.exe    Isolated transport unit tests
- libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll
                   MinGW runtime (keep these next to the exe)
- LICENSE, LEGAL   Project license and legal notes
- licenses/        GCC / libstdc++ license texts

The exe files are unsigned. Windows or SmartScreen may warn on first run.

Requirements
------------
- 64-bit Windows
- No extra install: put the folder anywhere and run from it.
- WS2_32.dll is a normal Windows system library (Winsock).

How to try two PCs on the same LAN
----------------------------------
On PC A:
  lan_node.exe --name Alice --dir alice-data --bind 0.0.0.0

On PC B:
  lan_node.exe --name Bob --dir bob-data --bind 0.0.0.0

Each process prints its peer_id and the TCP/UDP ports it bound. When a
peer is seen you get a "peer-up" line. Commands on stdin:

  list
  target 192.168.1.10:48710
  send Alice hello
  quit

Multicast discovery uses 239.255.87.42:48710 with TTL 1 (same LAN only).
If multicast is filtered, use --no-multicast and "target <ipv4:udp-port>"
on both sides (the printed udp= port).

The text transport on the LAN is currently plaintext.

Why there is no Telegram.exe here
---------------------------------
The verified Linux desktop app is official tdesktop with a LAN session
hook. Official Windows tdesktop builds use MSVC, midl, and a large
library tree. A MinGW cross-compile from Linux was started (Qt 6.8.3
mingw_64, Svg, ShaderTools) and stopped on missing MinGW Boost plus
that remaining library/midl/MSVC-flag work. See
docs/building-windows-mingw.md in the source repository.
