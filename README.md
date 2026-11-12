# LanLink slim

A slim, serverless LAN messenger with a Telegram Desktop-style Qt Widgets
interface. Current source: **v0.14**. This is **LanLink**, not
Telegram: it does not log into Telegram and does not talk to Telegram's
servers.

Text is always sealed with the official `tde2e` envelope
(`encrypt_message_for_one` / `decrypt_message_for_one`). File-transfer
encryption is a setting and is **off by default** so a trusted LAN can move
files faster (2 MiB plaintext chunks). When it is on, file payloads stay
fail-closed under the same tde2e envelope and 32 KiB chunks. The wire marks
plaintext offers with `LLP1` so the receiver knows to accept clear chunks;
sealed offers are still decrypted. Encryption protects the payload, not
membership of the local network.

Chat bubbles, the emoji picker, and the bundled sticker pack all use
official Desktop emoji sprites (quantized PNG atlases, because Linux Qt 6.8
here has no webp plugin). Mixed text and CJK in bubbles go through the
sprite tokenizer so missing glyphs do not become tofu. Animated tgs/rlottie
stickers are not played. The picker closes after an emoji or sticker is
inserted, same as official Telegram Desktop.

## Features

- Auto-discover LAN peers (UDP multicast `239.255.87.42:48710`, TTL 1)
- Edit Profile covers photo, first name, last name, and bio. First/last/bio
  are announced to LAN peers; the photo is stored and shown only on this
  computer. Display name defaults to the machine hostname. No intro/start page
- The chat header and the peer profile show the other side's IP
- The interface language follows the OS locale (English, Simplified Chinese,
  Traditional Chinese). Override with `--lang zh`, `zh-Hant`, or `en`, or
  change it in Settings. Visible strings are translated, including
  "Write a message...", "Message deleted", menus, and settings
- Copying the data folder to another machine rotates `peer_id`
- 1:1 and group text, files, photos, official-style emoji, and a static
  sticker pack
- Photo, File, and Folder send live under the paperclip attach button,
  like official Telegram Desktop. Drag a file or folder onto the current
  chat to send it the same way. Drops on an empty chat area are ignored
- Right-click a bubble for a Telegram-style menu (Copy Text, Open,
  Cancel, Delete for everyone) instead of a raw system menu
- Incoming files land in `downloads/` under the portable working directory
- Folder tree send/receive with sanitized relative paths
- File transfers default to unencrypted 2 MiB chunks (window of 32),
  resume after disconnect or crash, 16 GiB cap. Encrypted mode stays at
  32 KiB sealed chunks. Cancel from the bubble; abort is final and does
  not resume. SHA-256 verification covers both modes. Incoming GUI offers
  wait for Receive / Decline before writing payloads
- Recall (撤回) a sent text, sticker, or file for 48 hours; peers see
  "Message deleted" / "消息已删除". The deleted pill is wide enough not
  to clip that label
- LAN groups: create, later add/remove members from the chat header,
  1:N text, and group file/folder fan-out (fail-closed per recipient)
- Night / day mode follows the main window, side menu, compose bar,
  popups, and later-opened dialogs. Wallpaper is cached (including a
  pre-dimmed night tile) so toggling does not rebuild the chat
- Minimize / maximize / close use the native Windows title bar, with no
  duplicate in-app caption controls
- Windows portable exe embeds a LanLink .ico (not the official Telegram
  logo)

## v0.14 highlights

- Long-message layout is cached; scrolling paints only visible lines.
- Offline history stays visible, with safe merging of stale contacts.
- File-offer actions do not overlap; senders wait for receiver acceptance.
- Stable peers no longer trigger repeated history writes or list rebuilds.
- Active transfers use a bounded 4 ms scheduling interval, not a 0 ms busy loop;
  file progress updates are throttled. Large transfers still consume bandwidth.

## Build

Qt 6.8.3 and official `tde2e` are required. The CMake test target also
uses Qt for UI-helper tests. Adjust the example dependency paths to your
installation. See [third-party notices](third_party/README.md).

Linux:

```bash
cmake -S . -B build-linux \
  -DCMAKE_PREFIX_PATH=/workspace/qt/6.8.3/gcc_64 \
  -DTDE2E_PREFIX=/workspace/deps \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j
./build-linux/lan_tests
./build-linux/LanLink --dir /tmp/lanlink-alice --name Alice
```

Windows (cross-compile from this Linux box):

```bash
cmake -S . -B build-win \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-x86_64.cmake \
  -DCMAKE_PREFIX_PATH=/workspace/qt/6.8.3/mingw_64 \
  -DQT_HOST_PATH=/workspace/qt/6.8.3/gcc_64 \
  -DTDE2E_PREFIX=/workspace/mingw-deps \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-win -j --target LanLink
```

The portable working directory is the executable's folder when it is
writable, otherwise the current directory. `--dir` overrides that.
`--name` sets the display name. `--lang zh|zh-CN|zh-Hans|zh-TW|zh-Hant|en` selects
the interface language (otherwise the OS locale, or Settings).

## Layout

- `lan/` — protocol library (copied from the trim-e2ee transport, plus
  group file/folder fan-out). Independent of Qt.
- `app/` — Qt Widgets UI
- `resources/` — official Open Sans fonts, official compose/search icons,
  official chat wallpaper (`bg_initial.jpg`), LanLink app icon

This main branch contains the slim client, not the earlier full-desktop
fork. Earlier repository commits remain in Git history. Generated builds,
portable distributions, screenshots, and local peer data are not source
inputs and are not included. The bundled assets are ready to build;
`tools/prepare_assets.py` is an optional maintainer generator whose local
source paths must be adjusted before use.

## Tests

`lan_tests` covers identity, path sanitization, resume math, group
membership, 1:1 file/folder transfer, group file/folder fan-out, markdown,
long-text clipping, offline contact merging, and idle/transfer scheduling.
Run headlessly with `QT_QPA_PLATFORM=offscreen ./build-linux/lan_tests`.

The ub-mode tools (`crapper`, `dryer`, `mutator`) do not score C++.
