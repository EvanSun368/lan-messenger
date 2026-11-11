# Telegram source + LAN integration spike

This is a first vertical slice, NOT the full Telegram UI and NOT a release.
It actually adapts `PaintBubbleGeneric` from Telegram Desktop's
`Telegram/SourceFiles/ui/chat/message_bubble.cpp` at commit
`c4bd064bcc84a359179e191fa6dfd216acda504f` (7.4.1).

## Source and licence map

- `vendor/telegram_bubble_geometry.h`: adapted GPL-3.0-or-later Telegram source;
  original generic fill/corner/shadow/tail geometry retained, style radius constants
  changed to parameters. Original copyright and LICENSE/LEGAL retained.
- `src/bubble.h`: new Qt painter adapter for that algorithm.
- `src/main.cpp`: new minimal two-peer chat harness, not Telegram's conversation UI.
- `backend/node.*`: LanLink 0.5.2 networking/history/ACK queue copied from the current
  source; not Telegram networking. No Telegram account/API credential is used.
- `src/test_spike.cpp`: new geometry/render/network integration tests.

The combined prototype is distributed subject to the included GPLv3+ terms.
No Telegram logos or branding assets are included.

## Build

Qt 6 Core/Gui/Widgets/Network/Sql/Test and CMake 3.21+ are required.

```
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
cmake --build build -j2
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/TelegramLanSpike
```

Run on two reachable LAN machines and enter the other machine's IP and displayed
TCP port. Protocol is LanLink, NOT Telegram/MTProto. Local records use a separate
LanLink-Telegram-Spike data directory. Only the first discovered peer is selected
in this minimal harness. No auth/encryption: trusted test LAN only.

`--screenshot` is explicit demo data; it is not proof of network interoperability.
The normal UI sends using the real Node backend. The test suite separately checks
TCP delivery, ACK and persisted content when the environment allows sockets.

## What is not migrated

Telegram contact list, chat input, notifications, emoji/stickers, account/session,
rich message renderer, storage model, full lib_ui dependency tree and screen calls
are NOT integrated. File transfer/sharing/screen modules are NOT in this spike.
This is a source-reuse feasibility checkpoint, not an alternative production app.
