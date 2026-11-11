# Serverless LAN design note

This fork keeps the Telegram Desktop UI and style, but the intended network
path is a serverless LAN: no Telegram cloud account, and no self-hosted
MTProto server. Peers discover each other on the local network and talk
directly.

The first slice is implemented in `Telegram/SourceFiles/lan/`. It is tested.
It is **not** hooked into the tdesktop widgets yet.

## What works today

- Local identity (`peer_id` + display name) stored in two text files.
- Protocol encode/decode for announce, hello, text, ack, and bye.
- UDP multicast discovery (`239.255.87.42:48710`, TTL 1) and optional unicast targets.
- Direct TCP text chat between two `lan::Node` instances or `lan_node` processes.
- Unit tests and a two-process loopback smoke test (see `Telegram/SourceFiles/lan/README.md`).

## What does not work yet

- The official tdesktop GUI. A normal build of this tree still starts the
  upstream intro/login path and will contact official Telegram DCs.
- Files, voice/video calls, groups, channels, stickers, or secret chats on the LAN path.
- Encryption of the LAN transport (v1 is plaintext on the local network).
- NAT traversal or communication across routers; this is same-LAN only.
- A Windows tdesktop build was not produced here (too heavy for this environment).

## Exact next hooks into tdesktop

Do not rewrite the conversation list or history widgets. Adapt the session
layer so those widgets can stay.

1. **Stop official DC traffic first.**
   Built-in production DCs live in
   `Telegram/SourceFiles/mtproto/mtproto_dc_options.cpp`
   (`kBuiltInDcs`, `kBuiltInDcsIPv6`, and the test-DC tables).
   `Main::Account::start()` always calls `startMtp()` in
   `Telegram/SourceFiles/main/main_account.cpp`, which constructs
   `MTP::Instance` and begins connecting. LAN mode must skip `startMtp()`
   (or install an `MTP::Config` with an empty DC list and never call
   `requestConfig()`). Until that is done, a vanilla build will still try
   `149.154.*.*` on port 443 and the QR/phone flow that talks to Telegram.

2. **Replace the intro/login entry with a local name.**
   `Telegram/SourceFiles/intro/intro_start.cpp` (`StartWidget::submit`)
   currently goes to `QrWidget` / phone login
   (`intro_qr.cpp`, `intro_phone.cpp`). That path must not run in LAN mode.
   Swap it for a display-name form that calls
   `lan::load_or_create_identity` and then opens the main window.

3. **Create a local `Main::Session` without an `MTPUser` from Telegram.**
   After cloud login, the app calls
   `Main::Account::createSession(const MTPUser &user, ...)`
   (`main_account.cpp`, `main_account.h`).
   `sessionExists()` is what
   `Telegram/SourceFiles/core/application.cpp` and
   `Telegram/SourceFiles/window/window_controller.cpp`
   use to show the main UI instead of intro.
   LAN mode needs a synthetic self-user and a session object so the existing
   window stack can come up.

4. **Map LAN peers onto the dialogs list and history.**
   Discovered `lan::PeerInfo` values should become local `UserData` /
   `History` entries in `Telegram/SourceFiles/data/` and appear in
   `Telegram/SourceFiles/dialogs/`.
   Outgoing text currently goes through `Telegram/SourceFiles/api/api_sending.cpp`.
   Intercept that (or the history compose path) and call `lan::Node::send_text`.
   Incoming `lan::IncomingText` should be injected with
   `History::addNewLocalMessage` (`Telegram/SourceFiles/history/history.cpp`,
   already used for locally created items).

5. **Branding.**
   Keep `LICENSE` and `LEGAL`. Do not present this product as official
   Telegram. The intro title in `intro_start.cpp` is still the upstream
   "Telegram Desktop" string and must change when the UI hook lands.

## Suggested build flag

A compile definition such as `LANLINK_SERVERLESS` should be the single
switch that:

- skips `Account::startMtp()` / empties built-in DCs;
- shows the local-name intro;
- starts a process-wide `lan::Node`;
- routes send/receive as above.

Until that flag is wired, use `lan_node` to exercise the transport.
