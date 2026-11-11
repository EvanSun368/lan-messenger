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
- Protocol encode/decode for announce, hello, encrypted text, ack, and bye (protocol version 2).
- UDP multicast discovery (`239.255.87.42:48710`, TTL 1) and optional unicast targets.
- Direct TCP text chat between two `lan::Node` instances or `lan_node` processes.
- Official tde2e end-to-end encryption for 1:1 LAN text (see below).
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
    with `lan::Node::send_text`, which encrypts the body with official
    tde2e before it is written to TCP. Incoming ciphertext is decrypted
    before `History::addNewLocalMessage`.
  - The default window title uses **LanLink**, not "Telegram Desktop".

The single switch described in the original plan is `LANLINK_SERVERLESS`
(default ON). `Lan::kServerless` in `lan/lan_mode.h` is the corresponding
compile-time constant. Official DC emptying and MTP send no-ops are
unconditional in this fork, so a stock binary cannot silently fall back
to Telegram cloud even if a target misses the define.

`LANLINK_TRIM_CLOUD` (default ON) cuts official cloud features that this
fork does not use: multi-account, official phone/QR login and WebAuthn /
FIDO2 (passkeys), payments, Passport, Premium / Stars / gifts, Mini Apps
/ WebView, Instant View, Stories, channel/group statistics, official chat
export, on-device translation, and spellcheck. Calls and the wallet were
already disabled separately (`LANLINK_DISABLE_CALLS`,
`DESKTOP_APP_DISABLE_WALLET`). Animated stickers / tlottie and the file
sharing UI path are kept. Leftover official widgets may still be compiled
in; the main entry points are compile-time guarded so they no longer take
significant UI space or start a cloud flow.

## LAN end-to-end encryption

LAN text is protected with the official tdesktop `tde2e` API, not a
homemade cipher. Each peer generates a `tde2e` private key
(`key_generate_private_key`). The official mnemonic export
(`key_to_words` / `key_from_words`) is stored next to the LAN identity
in `lan_identity/e2e_words`. The 32-byte official public key is carried
in UDP announces and in the TCP Hello.

Once both sides have the other's public key, each message body is sealed
with `key_from_ecdh` + `encrypt_message_for_one` and opened with
`decrypt_message_for_one`. That is the same official one-shot ECDH
envelope used by `TdE2E::TemporaryKeyPair`. Plaintext is never written
to the TCP or UDP path for normal chat messages. If the public-key
exchange has not finished, `send_text` keeps the body queued in memory
and does not send it; there is no plaintext fallback.

Discovery and the TCP connection itself stay serverless: UDP multicast
`239.255.87.42:48710` (TTL 1) and direct TCP. Official secret-chat and
tde2e *call* machinery that depend on Telegram DCs or an out-of-band QR
channel are not used. The official `handshake_create_for_bob` /
`handshake_create_for_alice` APIs require an alternative channel to
verify keys (typically a scanned QR). That verification is not wired up
here, so a machine already on the LAN can still present a public key.
Encryption protects the path against passive eavesdropping. Confirm that
the discovered peer is the intended machine (display name and presence
on the expected network) before sending sensitive text. 1:1 text is
what this pass covers; groups, files, and stickers-on-the-wire remain
follow-ups.

## What does not work yet

- Files, voice/video calls, groups, channels, or stickers on the LAN path.
- NAT traversal or communication across routers; this is same-LAN only.
- A packaged **Linux** desktop binary can be built against system Qt 6.8
  (see [building-linux-packaged.md](building-linux-packaged.md)). `lan::Node`
  now has a Winsock port (`socket_compat.h`) and can be MinGW-cross-compiled
  as `lan_node.exe` / `lan_tests.exe` (see
  [building-windows-mingw.md](building-windows-mingw.md)). A full Windows
  tdesktop GUI package is not available from this Linux box yet: official
  Windows builds are MSVC + midl + a large Libraries tree, not MinGW.
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
  still in the tree. With `LANLINK_TRIM_CLOUD` they are not offered as
  intro steps; the startup path is the local display-name form. The intro
  field is still labelled "First name", and leftover App ID / window class
  values still say TelegramDesktop.

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
| Official tde2e key + encrypt/decrypt | `lan/e2e.cpp` (`tde2e_api`), used by `lan/node.cpp` |
| Cloud-feature cuts | `LANLINK_TRIM_CLOUD` in `Telegram/cmake/telegram_options.cmake` |
| Branding | `lan/lan_mode.h`, intro title, `window/main_window.cpp` |

A packaged Linux desktop binary is documented in
[building-linux-packaged.md](building-linux-packaged.md). `lan_node` and
`tests/smoke_two_processes.sh` remain the isolated transport checks.
