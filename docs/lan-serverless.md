# Serverless LAN design note

This fork keeps the Telegram Desktop UI and style, but the intended network
path is a serverless LAN: no Telegram cloud account, and no self-hosted
MTProto server. Peers discover each other on the local network and talk
directly.

The transport lives in `Telegram/SourceFiles/lan/`. The first UI/session hook
is also in that directory (`lan_mode.h`, `lan_ids.*`, `bridge.*`) and is
wired into the official intro, account, session, and compose-send paths.

A stock compile of this fork defines `LANLINK_SERVERLESS` (CMake option,
default ON) and does not require official Telegram `api_id` / `api_hash`.

## What works today

- Local identity (`peer_id` + display name) stored under the tdesktop
  working directory in `lan_identity/`, or under a standalone `--dir` when
  using `lan_node`.
- Protocol encode/decode for announce, hello, text, ack, and bye.
- UDP multicast discovery (`239.255.87.42:48710`, TTL 1) and optional unicast targets.
- Direct TCP text chat between two `lan::Node` instances or `lan_node` processes.
- Unit tests and a two-process loopback smoke test (see `Telegram/SourceFiles/lan/README.md`).
- **UI hook (this pass):**
  - Built-in official DC addresses are not installed. MTP `requestConfig()`
    and `sendRequest()` are no-ops, so a desktop build does not open
    sockets to Telegram DCs.
  - The intro screen is a local display-name form titled **LanLink**. It
    does not offer phone or QR login.
  - If a local identity already exists, `Main::Account::start()` fabricates
    a `Main::Session` from that identity and skips the intro.
  - Otherwise the name form calls `lan::load_or_create_identity` and
    `Lan::StartLocalSession`.
  - Discovered `lan::PeerInfo` values become local `UserData` / `History`
    entries and are added to the dialogs list (`History::clearFolder()` +
    `updateChatListExistence()`).
  - Outgoing compose text is intercepted in `ApiWrap::sendMessage` and sent
    with `lan::Node::send_text`. Incoming `lan::IncomingText` is injected
    with `History::addNewLocalMessage`.
  - The default window title uses **LanLink**, not "Telegram Desktop".

The single switch described in the original plan is `LANLINK_SERVERLESS`
(default ON). `Lan::kServerless` in `lan/lan_mode.h` is the corresponding
compile-time constant. Official DC emptying and MTP send no-ops are
unconditional in this fork, so a stock binary cannot silently fall back
to Telegram cloud even if a target misses the define.

## What does not work yet

- Files, voice/video calls, groups, channels, stickers, or secret chats on the LAN path.
- Encryption of the LAN transport (v1 is plaintext on the local network).
- NAT traversal or communication across routers; this is same-LAN only.
- A packaged **Linux** desktop binary can be built against system Qt 6.8
  (see [building-linux-packaged.md](building-linux-packaged.md)). A Windows
  package was not produced in this pass; `lan::Node` is still POSIX-only.
- `lan::Node` still uses POSIX sockets (`sys/socket.h`, `poll`, `fcntl`).
  A Windows desktop build will need a Winsock port of `node.cpp` before it
  can link.
- Multicast discovery is unreliable when the node is bound only to
  `127.0.0.1`. Binding `0.0.0.0` works on a real LAN; the loopback smoke
  test uses a unicast `target`.
- Chat history is not specifically persisted as LAN transcripts across
  restarts (tdesktop local storage may keep some items if the same
  synthetic user ids come back; this is not guaranteed).
- MTP is constructed (the session layer expects an `MTP::Instance`) but never
  connects. `dcstate()` / `dctransport()` report a connected / empty transport
  when there is no MTP session, so the official connecting banner does not
  abort the process. After a local session starts, contacts and the chats list
  are marked loaded (official MTP dialogs/contacts fetches are no-ops) so the
  sidebar does not stay on "Loading...". Discovered peers are fully loaded
  before `setIsContact`, given a non-zero chat-list date, and added to the
  dialogs list even before the first message. Opening a chat skips
  official `GetHistory` (which would never complete) so in-app bubbles
  from `addNewLocalMessage` are visible.
- Leftover official login widgets (`intro_qr.cpp`, `intro_phone.cpp`) are
  still in the tree but are not the startup path. The intro field is still
  labelled "First name", and leftover App ID / window class values still
  say TelegramDesktop.

## Hook map

| Step | Where |
| --- | --- |
| Empty official DCs | `mtproto/mtproto_dc_options.cpp` (`constructFromBuiltIn`) |
| Do not start MTP sessions or `requestConfig` | `mtproto/mtp_instance.cpp` |
| `dcstate` / `dctransport` without an MTP session | `mtproto/mtp_instance.cpp` |
| Drop MTP `sendRequest` | `mtproto/mtp_instance.cpp` |
| Do not restore a leftover official session or log out on MTP failure | `main/main_account.cpp` |
| Skip `AppConfig` network refresh | `main/main_app_config.cpp` |
| Skip session-constructor MTP requests | `main/main_session.cpp` |
| Local-name intro | `intro/intro_start.cpp` |
| Fabricate `Main::Session` + start `lan::Node` | `lan/bridge.cpp` (`Lan::StartLocalSession`) |
| Peer list → dialogs | `lan/bridge.cpp` (`ensurePeer`) |
| Send / receive text | `apiwrap.cpp` (`ApiWrap::sendMessage`) + `lan/bridge.cpp` |
| Branding | `lan/lan_mode.h`, intro title, `window/main_window.cpp` |

A packaged Linux desktop binary is documented in
[building-linux-packaged.md](building-linux-packaged.md). `lan_node` and
`tests/smoke_two_processes.sh` remain the isolated transport checks.
