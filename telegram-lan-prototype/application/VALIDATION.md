# Integration status and historical validation

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

## Previous stage: source-derived Message identity and original auxiliary controls

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

## Previous verified stage: ordinary Dialogs/History list forks and original settings

FULL now displays a source-derived ordinary DialogsInner fork in real
Ui::RpWidget/ScrollArea rather than a visible QListWidget. Names/previews use
original Ui::Text::String with upstream semibold/default text styles. The hidden
Qt item model remains LAN storage, not the rendered list. Source pointer/key
branches support real selection, repeated activation, filtering and scrolling.

The live history canvas uses a source-derived ordinary ListWidget layout/search/
anchor core via explicit LAN Elements and LanHistoryLayout. Existing QWidget
text/image/file rows remain interactive and allocated. This is NOT complete
HistoryView, nor memory virtualization; the independent painted-element path is
not misrepresented as the application's QWidget painting path.

Settings now use original GenericBox/LayerManager/InputField/FlatLabel/Checkbox/
SettingsButton, retaining five profile fields, font action and notification toggle.
Save failure retains the panel; cancel does not save profile edits. Wide-to-narrow
resizing reflows original content and recenters the original layer. Test-discovered
x=-20 clipping was fixed using BoxLayerWidget::parentResized, not manual row layout.
A scroll-generated synthetic mouse event overriding End-key selection was also
fixed; original-list test passed four consecutive targeted runs. Fractional-DPI
pixel tests now correctly convert logical row coordinates to device pixels and
sample inside the viewport clip, retaining the actual active-row color assertions.

Fresh serial verification (Qt6.11.2): FULL20/20 CTest groups60.98s; default20/20
43.34s (new original-only component cases intentionally QSKIP in default mode).
FULL UI125/150/200% each23 passes:15.170s/14.734s/16.859s. Latest PASSED marker and
logs are in full-ui-probe/verification-latest. Original settings targeted suite
5/5 across four runs; history-list core4/4; actual notification UI6/6. The old
file-only ASAN evidence is historical, not a new whole-UI ASAN acceptance.

Actual FULL screenshots: application/screenshots/original-shell-{wide,narrow,menu}.png
and original-settings-box.png. These use demo conversation data; the standalone
original-settings-narrow.png is a400px test host with demo profile fields. Actual
file/group images were refreshed from real-node tests. No Windows-native image,
Windows binary, complete original UI parity, main edit or remote publication.

Still open: full original Session/HistoryItem/Message/Element/RowPainter coupling,
complete private/group message presentation, true history virtualization, most
transfer/sharing/group/contact-profile dialogs, dark theme and native Windows
acceptance. No fake Telegram Session, cloud login or successful stub operations.
Source ledgers: application/telegram/{DIALOGS_INNER_SOURCE_SCOPE,
LAN_HISTORY_LIST_SOURCE,PROFILE_BOX_SOURCE_SCOPE}.md.

## Previous verified stage: original-control shell (2026-11-11)

FULL mode now uses actual unmodified upstream DefaultTitleWidget, IconButton,
PopupMenu, ScrollArea, SingleLine InputField/CrossButton and RpWidget in its shell,
in addition to original compose InputField and message FlatLabel. Original style
excerpts and icon masks are compiled by original codegen_style. This is not the
complete original Dialogs/HistoryView/MainWidget: LAN controller/layout/list host,
file cards and auxiliary dialogs remain custom. Exact provenance and boundaries:
application/telegram/ORIGINAL_SHELL_PORT.md and HISTORY_SESSION_BOUNDARY.md.

Fixed two real integration defects: zero-width original buttons under Qt layout,
and PopupMenu ownership destroying persistent command models after first close.
Three menus now repeat-open safely via independent action copies. Regression tests
also exercise original controls' visible geometry, actual send clicks, notification
navigation, long texts, IME, history and preserved file/ACK behavior.

Fresh serial verification using restored Qt 6.11.2 in BOTH modes: FULL 17/17 CTest
(53.20s), default 17/17 (41.30s); FULL UI 125/150/200% each 23 passes (12.794s,
13.473s,13.151s). Evidence: full-ui-probe/verification-latest/PASSED and logs.
The previous ASAN file-only run is historical, not a new shell ASAN acceptance.
Upstream git remains clean. No main changes, remote publication or Windows build.

Actual FULL screenshots: application/screenshots/original-shell-{wide,narrow,menu}.png.
These use demo conversation data; the menu is a standalone original widget grab.
File/group captures were refreshed from real-node tests. No Windows-native image,
full Telegram parity or resolution of the original Windows crash is claimed.

## Previous verified stage: durable remote file results

Direct/group offers now negotiate result_version=1. Receiver results are produced
only by actual states: completion requires SHA256 validation and final publication;
explicit rejection is distinct, while pause/failure remain retryable. Sender
validates original peer/host, transfer ID, size/hash, exact version and sequence,
persists before ACK and rejects stale/conflicting results. Local terminal metadata
and pending result share one atomic JSON snapshot; pending results retry across
restart. Late local callbacks cannot overwrite remote terminal states.

UI now shows each recipient's real outcome, including Alice's Bob-completed and
Carol-rejected cards. Sender-side remote completion never enables a local received
file Open action. Old release/finished sending remains unconfirmed; shared-directory
downloads explicitly retain legacy mode instead of advertising unsupported results.

Fresh final verification: FULL 17/17 groups (43.95s), default 17/17 (38.19s),
125/150/200% FULL UI each 22 passes. A separate AddressSanitizer build ran test_files:
29 passed (4.178s), no reported address errors. Prebuilt Qt was not instrumented and
leak detection was disabled; this is not exhaustive proof of all lifecycle paths.
Logs: full-ui-probe/verification-latest and verification-asan.

New actual FULL screenshots: application/screenshots/telegram-group-file-
{sender,bob,carol}.png. These now show matching terminal outcomes, not earlier
unconfirmed sender snapshots. No main branch edits or remote publication.

Limits: no cryptographic identity authentication; original-host binding may reject
old results after an IP change; file publication and the JSON snapshot are not a
cross-file atomic transaction. Full Telegram HistoryView, Windows acceptance,
virtualized history/full-database search and dark theme remain outside this stage.
See application/review/file-result-receipts.md for exact scope.

## Previous verified stage: group file receive routing

Upgraded peers receive group files in the correct group conversation, including a
first-file group declaration. Versioned context validates structure, unique
membership and both sender/receiver membership. Existing group names/membership
cannot be replaced by file or text declarations. Unknown groups follow the
existing unauthenticated LAN first-use trust model; this is NOT device identity
authentication. Old no-context offers stay direct; old ACKs explicitly leave
remote group routing unconfirmed rather than claiming support.

Fresh serial verification: FULL 17/17 groups (43.51s), default 17/17 (37.34s),
FULL UI 125/150/200% 22 passes each. Protocol focused tests: 26 passed. Real
three-node tests cover first-file groups, independent accept/reject, restart,
unknown-member reply remaining queued, strict context/legacy behavior, failed
persistence, replay and shared-response injection. Adjacent peer/group notification
callbacks were tested destroying their Node: a dropped ACK remains queued and
same-ID retry after restart succeeds without duplicate history.

Screenshots: application/screenshots/telegram-group-file-{sender,bob,carol}.png.
These are actual test nodes, not simultaneous final sender/receiver receipts.
The existing file_release protocol does not report distinct rejection/completion
results back to the sender; a sender card is not the receiver's final status.
A full remote final-file-result receipt flow remains to be implemented. A stale
invitation status-bar hint after rejection is a nonblocking UI follow-up.

No main changes, remote publication, Windows build or full Telegram parity claim.
Original upstream checkout is unchanged. See review/group-file-trust-boundary.md.

## Previous verified stage: file timeline and cursor history

- Real file cards now appear in conversation history. Offers remain waiting until
  explicit receive/reject; selecting Receive opens a destination chooser. Backend
  state/byte counts drive cards; sender sent is not receiver-validated completion.
- Local file-event mapping is transactional and deduplicated. Transfer snapshots
  persist with explicit save-error feedback; completed files are rehashed on restart.
  SQLite history failures warn without freezing an already visible live card.
- Exclusive (time,id) history cursors support more than 1000 total records. Loading
  earlier pages preserves a message/offset anchor and expanded long text. Backdated
  arrivals have bounded automatic reveal; distant ones show an older-message notice.
- Fresh serial verification: FULL 17/17 CTest groups (42.41s), default 17/17 (36.30s).
  FULL UI at 125%,150%,200%: 22 reported passes each. Logs and PASSED marker are in
  full-ui-probe/verification-latest. Actual FULL file screenshots are in
  application/screenshots/telegram-file-timeline-{sender,receiver}.png.
- Full HistoryView/Session, virtualized history, full-database search, group-file
  receiver context, dark theme, batch file preview and Windows acceptance remain.
  New file/history components are LAN integration code, not falsely attributed to
  Telegram's complete widgets. Original upstream checkout remains unmodified.

# Source lab validation

Linux Qt6.8.3/GCC14.2:
- CMake application and all test targets built.
- Seven CTest groups passed after actual module integration.
- UI suite additionally verifies TelegramInputAdapter is the real editor, IME
  composition does not send, and committed Ctrl+Enter sends exactly once over TCP.
- Input component: four focused functional tests passed.
- Dialog component: seven focused functional tests passed.
- Emoji source engine: official keyword results and actual popup search/select/clear tests passed.
- 33 upstream engine/generator/data/header files byte-verified against pinned source.
- Actual Qt chat screenshot generated and visually inspected.

Not tested: Windows binary, physical two-machine LAN, native Windows notification,
Windows reported receive crash, final polished Telegram UI. Screenshot uses demo data.

No build evidence should be taken as complete Telegram component parity.

## Optional FULL build: original Ui::InputField integrated

Linux Qt 6.11.2 / GCC 14.2, `LANLINK_FULL_TELEGRAM_UI=ON` via full-ui-probe:
- Complete unmodified lib_ui, lib_base, lib_crl/rpl and original generated style,
  palette and emoji targets built and linked into the actual LanLink application.
- MainWindow uses original Ui::InputField through a thin compatibility surface;
  networking, drafts, send button, emoji insertion and font settings remain wired.
- All **7 CTest suites passed** (22.26 seconds); see ../full-ui-probe/ctest-full.log.
- UI tests verify original InputField type; IME preedit cannot send and committed
  text sends once over TCP; Enter/CtrlEnter mode; UTF-8 exactly 131072-byte Chinese
  message delivers; appending emoji exceeds limit and preserves draft; font size
  remains reflected in actual character formatting after editing.
- Full application screenshot `screenshots/original-input-field.png` generated;
  process exited **0** (../full-ui-probe/run-application.log).
- Original input editor's real emoji resources and WebP plugin are present.
- Shutdown crash caused by qApp being null during QObject child cleanup was
  reproduced and fixed; post-fix screenshot and complete test processes exit 0.

Default OFF build continues to use the extracted-algorithm QPlainTextEdit adapter;
these full-source claims apply ONLY to the optional full build. Upstream source
checkout remains unmodified. Other UI/transport are not a full Telegram rewrite.

## Integrated workspace/text stage

- FULL: 9/9 CTest groups passed, 25.22 seconds. Default: 9/9 groups passed,
  with full-only lifecycle/text cases intentionally skipped.
- Real message rendering tested for original text preservation, actual wrapped
  height, visible ink in tail region, selection/whole-message copy and fonts.
- 400px workspace returns to conversation list and keeps message avatars inside
  viewport. Screenshot-run process exit: 0.
- Fixed approved image recipient survives current-conversation changes, verified
  with separate Bob/Carol real loopback Nodes; only Bob receives the image.
- Original InputField preedit clearing tested on draft replacement, clear and
  focus loss. Real native Windows IME is still unverified.
- New screenshots: telegram-integrated-wide.png and
  telegram-integrated-narrow-{chat,list}.png. These use labelled demo data.

## ACK, long-text folding and service-lifecycle stage

- FULL 14/14 CTest groups passed (see latest ctest-final-stage.log).
- Qt scale factors 1.25/1.5/2: UI suite 20 reported passes each, without runtime
  instrumentation. This validates Linux offscreen logical scaling, NOT physical
  Windows mixed-monitor DPI or native IME/tray behavior.
- Corrected scroll-test prerequisite: 40 was clamped to max35 during incomplete
  layout, which was still at bottom. The test now waits max>64, immediately
  verifies position40, and retains strict reading-position preservation checks.
- Real group partial result screenshot is based on two loopback devices: one ACK
  after save, one offline. UI displays 1/2 and per-device result, never "read".
- Receipt ledger transaction failure injection preserves outbox and suppresses
  delivered events; retry uses same ID, preserves acknowledged targets and does
  not duplicate either endpoint's history. Old message evidence remains Unknown.
- Original text renderer folding leaves text/layout intact, supports expand,
  complete copy and UTF-8 atomic save. Expanded long-text height/ink tests remain.
- Sharing response identity, image recipient snapshots, callback-owner deletion,
  modal receive ACK ordering, verification cancellation, duplicate fetch and
  symlink escapes have specific regression cases.

Still unaccepted: complete P0 matrix, file timeline cards/batch preview, historical
pagination, dark theme, pin/mute, native Windows packaging and original reported
Windows shared-file crash. See review/spec-acceptance-matrix.md.
