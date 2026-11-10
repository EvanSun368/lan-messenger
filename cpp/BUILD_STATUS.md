# Build status — 2026-11-10 feedback revision

This is native C++/Qt 0.5.1 source, not a Windows executable.
The previously provided runnable portable package was Python 0.4.0, not this native revision.

The latest previous Windows attempt, https://github.com/EvanSun368/lan-messenger/actions/runs/42796527086, again failed before any build step started. The earlier confirmed blocker was account payment/spending limits; no new Windows compilation result is available. No billing settings have been changed. An older native Windows build passed, but it does not contain current fixes and must not be presented as this revision.

Final local validation for this revision is recorded in VALIDATION.md. Linux tests and offscreen GUI screenshots do not establish Windows tray notification behavior, high-DPI layout, or physical two-machine LAN acceptance.

Reported shared-file crash has not yet been reproduced in available automated tests. Request exact version and trigger steps before attributing it to a specific cause. Do not report the crash as resolved merely because tests pass.
