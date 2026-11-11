# Validation checkpoint

- Linux x86_64 GCC 14.2 + Qt 6.8.3: CMake configure and both targets built successfully.
- QtTest: 3 functional test cases passed (geometry callbacks, rendered pixel boundary,
  two real TCP nodes with message/ACK/history). Qt's init/cleanup bring printed total to 5.
- Executable launched offscreen and screenshot generated and visually inspected.
- No Windows build attempted; known GitHub billing blocker remains.
- No multi-machine LAN/notification/file/screen validation. This prototype includes only
  the first text chat vertical slice, not the original complete LanLink features.

Build-environment notes: installed official Qt archive dependencies in a temporary location;
parallel aqt installer could not create its multiprocessing IPC socket, so its unmodified
archive verification/extraction functions were invoked sequentially. OpenGL headers were
obtained from Debian libglvnd development packages; system runtime libraries were used.
No OS/security settings were changed.
