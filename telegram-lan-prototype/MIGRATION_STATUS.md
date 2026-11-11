# Integration status and historical validation

## Current revision: 0.6.1 updates and app icon

The default 0.6.x prototype now has a user-visible 检查更新 action, optional
background auto-update, and an original LanLink window/tray/exe icon. Update
checks talk to GitHub Releases of the private `EvanSun368/lan-messenger`
repository and therefore need a token stored in Windows Credential Manager;
Linux still fails closed for secrets. Only `telegram-mail-vpn` packages are
selected, not 0.5.x cpp zips. Staged updates apply after quit or on the next
launch and are not a force-restart. See [UPDATES.md](application/UPDATES.md).
The caveats below still apply: this is a source integration lab, FULL UI is
Linux-only, and Windows runtime/update application has not been accepted on a
real Windows machine in this revision.

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
See [MAIL_SYNC_UI.md](application/MAIL_SYNC_UI.md), [MAIL_OAUTH_SETUP.md](application/MAIL_OAUTH_SETUP.md) and
[MAIL_SCOPE.md](application/MAIL_SCOPE.md).

Managed-VPN mode explicitly selects a local interface/address and exact peer
IPv4 allowlist, permits 100.64/10 VPN addresses, disables LAN discovery and refuses
fallback. Its diagnostics do not install or validate a VPN. Application peer
identity remains unauthenticated and there is no application E2EE. OS routing and
VPN/firewall ACLs remain the operator's responsibility. See [VPN_USAGE.md](application/VPN_USAGE.md).

The default Qt build is independent of the full upstream checkout. FULL mode
imports the pinned original lib_ui/codegen sources and is currently **Linux-only**.
The production credential backend is currently **Windows Credential Manager only**;
Linux/macOS authentication fails closed. Consequently the tested FULL UI and the
credential-capable platform do not yet form a verified end-to-end product.
Windows build/runtime/credential acceptance, real Google/Microsoft/other mailboxes,
OS tray delivery, real VPN/cross-city two-device behavior and the earlier reported
Windows receive crash remain unverified. This is a source integration lab, not a
production release or a complete Telegram Desktop application port.

Final integration verification is recorded in [INTEGRATION_VALIDATION.md](application/INTEGRATION_VALIDATION.md).
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

# Telegram Desktop → LanLink：迁移调查记录

## 当前状态

已从官方 https://github.com/telegramdesktop/tdesktop 克隆完整浅层工作树。
基线：`c4bd064bcc84a359179e191fa6dfd216acda504f`，源码版本 7.4.1。
已建立可编译运行的独立源码移植实验版 application，真实接入部分上游模块。原始 LanLink 工作树及 main 未改。
这不是完成的 Telegram 界面迁移产品；详见下方各阶段范围及 application/README.md。

## 建议实现路径

保持局域网工具的用户意图：不需要 Telegram 账号，不把文件或聊天发往 Telegram 云。
因此选“Telegram Desktop 界面与交互派生 + 独立 LAN 后端”，不是部署原版 Telegram。
必须先建立一条不触发 Telegram 网络和登录流程的 LAN 启动路径，再逐个接入数据模型。

1. 固定官方源码及子模块提交，保留 GPLv3+ 许可与版权；使用自己的应用名和图标。
2. 建立 LAN 模式入口。与现有 Node 之间使用 Peer/Conversation/Message/Transfer/Profile 模型，不模拟 Telegram 登录。
3. 接入真实聊天列表和历史展示，只做双节点文本贯通；验收确认不连接 Telegram 服务。
4. 接入离线发送队列、图片、通知、字体及输入组件，保持消息确认/去重语义。
5. 接入文件/共享模块。目录共享和 LAN 发现仍是自有逻辑；不能假定 Telegram 提供。
6. 投屏单独对接；原 Telegram 通话的服务器信令不能直接替代现有 LAN 投屏。
7. Windows 全新构建、安装依赖扫描、双机网络与长消息/续传/撤销/权限验收。

## 已确认的耦合

- `Telegram/SourceFiles/history/view/history_view_list_widget.h` 引入 `mtproto/sender.h`、Telegram data 与 history model。
- `Telegram/SourceFiles/main/main_session.h` 持有 `MTP::Instance`、`Data::Session`、`ApiWrap`、Uploader、DownloadManagerMtproto。
- `Telegram/SourceFiles/dialogs/dialogs_inner_widget.cpp` 直接观察 Session 的 chatsList、contactsLoaded、itemRemoved 等流。
- `Telegram/SourceFiles/config.h` 说明原 Telegram API 需要自己的 api_id/api_hash。纯局域网路线不应依赖这些身份凭据。
- lib_ui、lib_base、lib_rpl、lib_crl 和代码生成、样式等为独立子模块，不是普通 Qt Widgets 文件复制即可编译。

## 限制

Windows Actions 已知受用户账户付款/额度阻碍，本轮未尝试绕过或修改账单。
完整 Telegram 界面移植是架构替换，无法将现有小型实现简单改名就完成。
向用户分发派生二进制时需要遵守 GPL 条件并提供相应源码；开源组件许可和品牌限制分别处理。
不应将“拿到源码”或“本地设计完成”报告为“已经迁移完成”。

## 后续实际实施

`prototype/` 现已实现并编译：从固定上游提交真实提取并改造
`PaintBubbleGeneric` 气泡几何算法，使用独立 Qt painter 适配器，并连接现有
LanLink Node 完成最小双节点文本聊天切片。保留逐文件来源和 GPL。
Linux 3项功能测试通过（Qt输出含初始化/清理共5项），含真实TCP消息/ACK/历史。
UI已启动并查看截图，但仅为工程验证壳，不代表最终界面或完整Telegram迁移。
原 Telegram Dialogs/History/输入控件/通知等尚未接入。

## 第二阶段实际实施

`application/` 是保留完整现有LanLink功能的独立实验副本，已将三个真实来源模块
接入实际执行路径：气泡绘制、会话排序和未读格式、输入提交/IME行为。
六组全量CTest通过（包括新加的真实界面输入、IME不误发、网络送达用例）。
另外输入4项和会话组件5项专项通过。Linux界面已启动并截图目检。
独立数据目录防止覆盖现有设置/历史。未改远端或main，也未尝试绕过Windows CI账单限制。

完整原版Ui::InputField和RowPainter仍未移植。lib_ui完整构建依赖生成palette/style/emoji、
lib_base/rpl/crl、私有Qt或Pango文本引擎，以及Linux DBus/KDE/XDG等平台库；应用列表仍
与Telegram Session/History/Peer相连。因此当前是有可运行成果的模块移植中间状态，
并非完整Telegram界面或已完成的重写产品。

## 后续：完整表情建议引擎与会话行绘制

已原样编译 lib_ui 的完整 emoji_suggestions 引擎，通过原版 codegen_emoji
和官方词典生成搜索数据，接入应用表情面板的英文关键词搜索。不是只复制表情文字，
也不是完整 Telegram 表情面板。构建复现源位于 emoji-component，逐文件来源见文档。
新增官方字典与搜索/选择/清空恢复测试，7 组 CTest 在该阶段全部通过。

已将 PaintRow 可脱离云端的绘制子集接入三类真实列表 delegate；完整 lib_ui 构建探针另行继续，
真实 Ui::InputField 尚未完成，不能将提取的输入逻辑称为原版完整输入框。

## 原版完整 InputField 真实应用集成已验证

不再只停留输入算法提取。LANLINK_FULL_TELEGRAM_UI 模式链接未修改的完整
lib_ui 并使用原版 Ui::InputField；薄兼容层接回现有 LAN 发送/草稿/字体流程。
Qt6.11.2 下完整应用7组CTest全过（22.26秒），截图运行退出0。
修复集成暴露的IME误发送与退出qApp空指针；真实TCP验证精确UTF-8 128KiB
中文消息及添加emoji超限保留草稿。默认小依赖6.8构建仍保留，非完整界面迁移。
其余会话/历史渲染仍是部分源码移植，不宣称完整Telegram客户端或Windows版本完成。

## 整体工作区与原版文字引擎迁移

FULL模式已采用单窗口会话列表/聊天双栏，列宽与窄屏规则提取自上游
window_session_controller.cpp::computeColumnLayout。窄屏提供返回列表，
最小化后打开会话能恢复。主窗口外观及LAN操作宿主仍是新适配代码。

聊天消息正文已真实使用未修改的Ui::FlatLabel/Ui::Text::String，而非QTextBrowser。
针对原版单实例32K字符上限按字素分片，提供整条复制；跨片拖选仍未实现。
原版默认FlatLabel样式minWidth=0不换行的问题由高度测试发现，宿主设为1修复，
新增每片高度和末尾区域实际绘制检查，未改上游或弱化断言。

集成审查修复：截图异步预览冻结原接收人，防止切会话错发；QScreen用弱引用；
输入法预编辑在替换草稿/清空/失焦时清理，防止候选残留到其他会话。
最新全量回归证据见full-ui-probe/ctest-integrated-view.log。

## 真实送达状态与长文折叠

新增delivery_batches/delivery_targets持久保存发送时收件人集合及真实ACK。
ACK证据与删除outbox同事务，数据库失败保留待发，不发假成功。旧消息Unknown。
消息气泡可查看已接收x/n设备与逐人错误，同ID重试只恢复未确认目标，不冒充已读。
原版文字渲染器支持约12行折叠/展开、全文复制及UTF-8保存，不截断原文。
125%、150%、200% Qt缩放UI回归通过；物理Windows与混合屏幕缩放尚未验收。
完整规范差距见application/review/spec-acceptance-matrix.md，不将阶段测试当作P0全部完成。
