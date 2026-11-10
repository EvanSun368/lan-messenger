# Validation record

2026-11-10 native C++ migration:
- Linux GCC14 + Qt6.8.3 compiled.
- Five CTest executables: core messages/profiles/images, file transfers, link URL validation, screen protocol, Widgets UI.
- Real Python 0.4 client <-> C++ headless peer: direct text, group reply, JPEG echo and public profile.
- File protocol fixture: Python send/receive against C++ wire protocol.
- Real node restart plus new TCP port and partial-file resume; no overwrite; share revocation; malformed input and path traversal checks.
- Actual Qt offscreen screenshots inspected, not image-generated UI mockups.

Windows CI is the authoritative packaged-build record. A passing Linux run is not a Windows pass. A demo startup screenshot is not a real two-computer screen capture/transfer test. No full hardware, multi-monitor, large-group throughput, antivirus signing, encryption, or authenticated-device acceptance is claimed.

## 0.5.1 feedback revision
- Rebuilt native executable and six CTest executables; all passed.
- Added notification unread/read lifecycle, emoji category/selection, and integrated conversation UI tests.
- Real legacy Python/C++ text, group, JPEG and profile protocol interoperability passed again.
- Actual Qt offscreen chat screenshot inspected; CSS descendant overmatching removed.
- Windows system tray notifications and focus behavior not tested on a real Windows desktop.
- The user's Python 0.4.0 receiving-side crash has not been reproduced; it is not established as fixed by native migration.

## Modern chat direction
User approved the modern QQ/WhatsApp-style chat appearance, superseding the earlier Feige-style conversation requirement. Contacts remain a separate window. Qt offscreen screenshots at 100/150/200% scaling were inspected/tested; this is not Windows rendering acceptance.
- Modern-message regression cases: click thumbnail opens original-size image; asynchronous layout scroll follows latest only while at bottom, preserves reading position otherwise; font changes recompute message size; 32 KiB unbroken text and long URL wrap; minimum chat window with history sidebar; test settings isolated from user settings. UI tests passed at 100/150/200% Qt scale factors.
- A slower ASAN run exposed a scroll-range timing race. The final revision restores after scrollbar range changes, retains the reading anchor through consecutive rebuilds, and cancels automatic positioning on user scrollbar actions. Three consecutive ASAN UI runs passed with state-based waits and unchanged position assertions; this was not a memory-safety sanitizer report.
