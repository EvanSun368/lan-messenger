# Validation record

2026-11-10 native C++ migration:
- Linux GCC14 + Qt6.8.3 compiled.
- Five CTest executables: core messages/profiles/images, file transfers, link URL validation, screen protocol, Widgets UI.
- Real Python 0.4 client <-> C++ headless peer: direct text, group reply, JPEG echo and public profile.
- File protocol fixture: Python send/receive against C++ wire protocol.
- Real node restart plus new TCP port and partial-file resume; no overwrite; share revocation; malformed input and path traversal checks.
- Actual Qt offscreen screenshots inspected, not image-generated UI mockups.

Windows CI is the authoritative packaged-build record. A passing Linux run is not a Windows pass. A demo startup screenshot is not a real two-computer screen capture/transfer test. No full hardware, multi-monitor, large-group throughput, antivirus signing, encryption, or authenticated-device acceptance is claimed.
