# LanLink: Telegram source integration laboratory

## Integrated stage: mail OAuth, polling/replies and managed VPN

The independent mail module now has Gmail, Outlook.com/Hotmail and Microsoft 365
OAuth authorization-code flow with S256 PKCE, loopback callback, refresh and
IMAP/SMTP XOAUTH2. Client registration belongs to the operator; no registrations,
real credentials or tokens are included. Manual/QQ/163/application-password
accounts remain available with verified TLS/STARTTLS only.

Mail adds per-account opt-in polling, UIDVALIDITY-aware cursors, bounded header
previews, privacy-preserving new-mail notices, separate account drafts and explicit
reply composition. Reply, chat export and polling never send mail. SMTP submission
still requires explicit review/confirmation; an unknown final outcome is never
automatically retried. Original Telegram controls are used in FULL mode through
adapters, while the mail controller/layout and account/settings UI are custom.
See [MAIL_SYNC_UI.md](MAIL_SYNC_UI.md), [MAIL_OAUTH_SETUP.md](MAIL_OAUTH_SETUP.md) and
[MAIL_SCOPE.md](MAIL_SCOPE.md).

Managed-VPN mode explicitly selects a local interface/address and exact peer
IPv4 allowlist, permits 100.64/10 VPN addresses, disables LAN discovery and refuses
fallback. Its diagnostics do not install or validate a VPN. Application peer
identity remains unauthenticated and there is no application E2EE. OS routing and
VPN/firewall ACLs remain the operator's responsibility. See [VPN_USAGE.md](VPN_USAGE.md).

The default Qt build is independent of the full upstream checkout. FULL mode
imports the pinned original lib_ui/codegen sources and is currently **Linux-only**.
The production credential backend is currently **Windows Credential Manager only**;
Linux/macOS authentication fails closed. Consequently the tested FULL UI and the
credential-capable platform do not yet form a verified end-to-end product.
Windows build/runtime/credential acceptance, real Google/Microsoft/other mailboxes,
OS tray delivery, real VPN/cross-city two-device behavior and the earlier reported
Windows receive crash remain unverified. This is a source integration lab, not a
production release or a complete Telegram Desktop application port.

Final integration verification is recorded in [INTEGRATION_VALIDATION.md](INTEGRATION_VALIDATION.md).
Older counts below are historical stage records, not acceptance of this revision.
No real mailbox credentials or external mail were used. Generate the disposable
TLS fixture locally before transport tests; its private key is never committed.
The GitHub integration is reviewed on a separate branch; no merge, main overwrite,
Windows binary or executable release is implied.

This is a **partial source-port experiment**, not a full Telegram Desktop fork UI,
not a release. The optional full build now links the complete original lib_ui and
uses original Ui::InputField, FlatLabel, title/search/action/menu/scroll controls;
the application/session/list host remains a partial LAN port.
The existing LanLink C++ 0.5.2 application/backend is retained, with additional actual
Telegram-derived modules wired into real execution paths:

| Module | Real upstream source reused | Integration |
| --- | --- | --- |
| Bubble geometry | Telegram Desktop `ui/chat/message_bubble.cpp`, `PaintBubbleGeneric` | Every chat message uses TelegramBubbleFrame and its painter adapter |
| Dialog model/unread counts | `dialogs/dialogs_list.cpp`; `ui/unread_counter_format.cpp` | Contact/message/group list ordering and badge formatting |
| Input editing, default OFF build | Extracted `input_field.cpp` logic | QPlainTextEdit adapter with submit predicate |
| Input editing, FULL build | Complete original lib_ui / Ui::InputField | Real editor in MainWindow, existing send/draft backend and IME gate; original styles/emoji/animation |

| Dialog row painting | `dialogs/dialogs_layout.cpp` basic PaintRow/PaintRowTopRight/PaintWideCounter flow | Real contact/message/group delegate; cloud/premium/rich-text portions excluded |
| Emoji suggestions | Complete original `emoji_suggestions.cpp` and generated official dictionary | Emoji popup English search and selection |

| Message text (FULL) | Original `Ui::FlatLabel` / `Ui::Text::String` | Actual message text, emoji, selection, safe long-message segments |
| Workspace columns (FULL) | `Window::SessionController::computeColumnLayout` rules | Single-window two-column and narrow-screen navigation |

| Shell controls (FULL) | Original DefaultTitleWidget, IconButton, PopupMenu, ScrollArea, SingleLine InputField/CrossButton | Actual title/search/menu/actions/scroll, custom LAN layout/controller |

## Latest stage: source-derived Message identity and original auxiliary controls

FULL now applies actual ordinary Message/Element presentation branches: private
chat has no author/avatar; incoming group runs show the author at the first row
and avatar at the last, with a reserved avatar column. Bubble attachment and
unwrapped-neighbor rules differ correctly. Original codegen tail masks and tail
painting are active; attached/final bubble bodies and text stay aligned. Short
history uses the original bottom-alignment rule. This remains a source port,
not transplantation of full Session/HistoryItem/Message classes.

Group creation, contact profiles, transfer records and sharing now open real
original GenericBox/LayerManager with original input/checkbox/text/link/scroll
controls. LAN controllers and their content remain newly authored. The transfer
progress bar is custom; sharing retains hidden legacy state holders, not a
visible legacy dialog. Accepted sharing results bind peer/token/directory;
subsequent draft edits cannot retarget a download, and reopening preserves drafts.
Native filesystem dialogs and explicit publication/download confirmations remain.

Real TCP tests exercise reject through the original layer and remote rejection
receipt (zero payload bytes), and sharing listings with stale-response rejection.
Modal original layers no longer silently mark obscured messages read. Two-node
notification tests cover user return and background close without stealing focus.
Group/private row tests verify changing the final avatar after live append.

Remaining: complete original Session/HistoryItem/Message/Element/ChatStyle and
media rendering; true memory virtualization; original emoji/font/delivery/image/
screen-share dialogs; dark theme and Windows-native packaging/acceptance. LAN
identity remains unauthenticated; acknowledgements are receipt, not remote read.
No cloud account/Session emulation, main branch edit or remote publication.

Source ledgers: telegram/MESSAGE_IDENTITY_SOURCE.md, LAN_HISTORY_LIST_SOURCE.md,
CONTACT_BOXES_SOURCE_SCOPE.md, TRANSFER_LIST_SOURCE_SCOPE.md,
SHARING_BOX_SOURCE_SCOPE.md. Pinned upstream remains unmodified. The legacy
ASAN evidence is historical, not acceptance of the entire new UI.

Fresh serial verification: FULL 24/24 CTest groups (67.66s); default 24/24
(47.29s), with original-only cases intentionally skipped in the default build.
FULL UI at 125%, 150%, 200% each passed 24 cases (18.326/18.015/20.061s).
The final PASSED marker and logs are in full-ui-probe/verification-latest.
An earlier default build ran out of /tmp disk space; generated FULL build data
was moved to the larger workspace with a same-path symlink, then the full matrix
was rerun successfully. No source workaround or test relaxation was used.

## Ordinary-list and settings stage

FULL mode now replaces the visible QListWidget with a source-derived ordinary
DialogsInner fork, actual Ui::ScrollArea/RpWidget and original Ui::Text::String.
The QListWidget remains hidden storage, not the displayed list. LAN filtering,
selection and repeated activation use the live backend model.

HistoryView's ordinary geometry/search/anchor mechanics are forked into the live
LanHistoryList canvas and LanHistoryLayout. Existing QWidget message/file elements
remain allocated and interactive: this is NOT complete HistoryView or memory
virtualization. The independent paint-element path is not falsely described as
the app's QWidget painting path.

Settings use original GenericBox/LayerManager/InputField/Checkbox/SettingsButton,
with the existing public-profile persistence, font action and notification toggle.
Other transfer/sharing/group/profile-view dialogs are still custom. See
`telegram/DIALOGS_INNER_SOURCE_SCOPE.md`, `telegram/LAN_HISTORY_LIST_SOURCE.md` and
`telegram/PROFILE_BOX_SOURCE_SCOPE.md` for provenance. Native Windows validation
and the complete original Session/HistoryItem/RowPainter migration remain open.

## Exact upstream references

Telegram Desktop commit `c4bd064bcc84a359179e191fa6dfd216acda504f`, version 7.4.1.
lib_ui submodule commit `42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4`.
Repository URLs: https://github.com/telegramdesktop/tdesktop and
https://github.com/desktop-app/lib_ui . See `telegram/` source comments and scope files.

GPL-3.0-or-later obligations apply to this combined derivative. Included LICENSE,
LEGAL and telegram/DESKTOP_APP_LEGAL preserve upstream terms. Qt remains dynamically
linked with its own applicable LGPL/GPL/commercial licence. No Telegram brand/logo
assets, API credentials, or remote account integration are included.

## What remains original LanLink or unported

- New/previous LanLink: networking, peer IDs, history database, ACK/offline queue,
  file transfer/sharing, screen sharing, profile, notifications,
  MainWindow/controller/layout and message-row host (FULL text and many shell
  controls use original lib_ui).
- Not ported: full Telegram `Dialogs::RowPainter`, rich message
  renderer, sticker packs, account/MTProto
  stack, calls/servers. Punctuation replacement is disabled to preserve code/logs.
- The current FULL shell now integrates real upstream title/search/buttons/menu/
  scroll components. This is not the entire original Dialogs or HistoryView shell.
  See `telegram/ORIGINAL_SHELL_PORT.md` for exact provenance and remaining custom UI.

## Safety and compatibility

Uses separate `.lanlink-telegram-lab` data and `LanLinkTelegramLab` settings by
 default; does not import existing Python/C++ history. No Telegram network is
used. Existing LAN messages remain unencrypted and peers unauthenticated: trusted
internal tests only. Previous reported Windows receive crash is still unresolved;
Linux success does not establish its fix. No new Windows binary is provided.

## Build

Qt6 Core/Gui/Widgets/Network/Sql/Test/Concurrent, CMake 3.21+, C++17 compiler:
```
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build -j2
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/LanLink
```
`--demo --view chat --screenshot path.png` uses labelled demonstration data and
never proves actual network behavior. Tests cover separate real loopback nodes.

## Next genuine migration step

Port the full generated style/text/emoji stack of lib_ui to obtain actual Telegram
rich input and row rendering, or decouple those UI classes from Session/Data.
The present extracted algorithms do not remove those dependencies. A full app
rewrite needs further engineering and real Windows verification, not a rename.

## Additional genuine source port

The original complete Telegram emoji suggestion engine and generated official
dictionary now back the emoji popup search (English keyword/shortcode). See
telegram/emoji/SOURCE_SCOPE.md. Existing category grid and font rendering remain
LanLink; custom/animated stickers and the original emoji panel are not ported.

## Optional original Ui::InputField build

Default `LANLINK_FULL_TELEGRAM_UI=OFF` preserves standalone Qt 6.8.3 compatibility.
To build the full original input component, configure through `../full-ui-probe`:

```
../full-ui-probe/configure-local.sh -DLANLINK_BUILD_APPLICATION=ON
BUILD_TARGET=LanLink ../full-ui-probe/build-local.sh
```

This requires the pinned original source checkout and real Qt 6.11.2, GLib/GIR,
Vulkan/JPEG/etc dependency chain documented in the probe. It is not yet a portable
Windows build configuration. The full build keeps LanLink's networking and other
features; it does not use Telegram's service or MTProto.

`telegram_full_input_adapter.*` is a thin API compatibility surface around the
original InputField, with application host initialization and an explicit IME
submission guard. The full build uses lib_ui's original emoji engine without
linking a duplicate engine. Font changes update the original field's per-editor
style, not only the outer widget font. InputField sources are unmodified.

## Whole workspace and original message text (FULL mode)

The FULL build now uses a single-window conversation/chat workspace. Responsive
column rules are ported from Telegram SessionController::computeColumnLayout;
window chrome and LAN wiring are new integration code. Original FlatLabel and
Text::String render actual message text, selection and emoji. Long messages are
segmented below the upstream 32K-instance ceiling without discarding source text;
whole-message copy is provided, but drag selection across segments is not.
See telegram/COLUMN_LAYOUT_SOURCE_SCOPE.md and MESSAGE_TEXT_SOURCE_SCOPE.md.

HistoryView's full Session/History/cloud model is NOT ported. File/share/screen
services and history storage remain LanLink implementations. The updated workspace
is more than an isolated component probe but not complete Telegram UI parity.

## Delivery evidence and current status UI

New local SQLite tables freeze each message's recipient devices and retain actual
ACK evidence. Receipt persistence and removing the queued copy are atomic; missing
legacy evidence stays Unknown. Outgoing message badges expose per-device results
and same-ID retry. No remote read receipt or Telegram cloud status is implied.
The tables are additive; old message content is unchanged.

FULL long messages offer 12-line fold/expand, complete copy and UTF-8 save.
Date grouping and display logic use the documented source-derived history module.
See review/spec-acceptance-matrix.md for unimplemented P0 items.
