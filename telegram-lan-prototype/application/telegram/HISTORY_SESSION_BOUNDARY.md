# HistoryView migration boundary and actual new port

Pinned Telegram Desktop c4bd064bcc84a359179e191fa6dfd216acda504f (GPL3+).

## Why a fake Session is not an acceptable adapter

history_view_list_widget.cpp:563 constructs ListWidget with non-null Main::Session.
Its initializer constructs EmojiInteractions with that session and a highlighter
with session->data(); lines 661 onward subscribe to concrete Data::Session view
repaint/resize/layout/item streams. ListDelegate (header:134 onward) supplies
Data::MessagesSlice, HistoryItem pointers, Window::SessionController, PhotoData,
DocumentData, reactions, polls and read-marking. Allowing listWindowOrNull alone
does not remove these direct session dependencies.

history_view_message.cpp imports API transcription/suggested posts, Core app,
HistoryItem components, Session/peer models, cloud media, reactions and premium.
Returning dummy objects from these interfaces would give misleading behavior and
risk unintended cloud operations. We do not create a Telegram Session or keys.

## Explicit alternative boundaries for a real LAN-backed view

1. HistorySource: stable LAN message IDs, display order, cursor pagination, immutable
   snapshots and insert/update/remove event stream. Existing Node::history is the
   source now; preserving selection/scroll by IDs needs a dedicated incremental
   controller rather than rebuilding all rows in the long term.
2. MessagePresentation: sender ID/display name, local display-zone timestamp,
   direction, text/entities, media descriptors, source-backed delivery status.
   Current HistoryMessage implements the minimal chronological/grouping slice.
3. MediaResolver: local image/file thumbnail and open callbacks, permission/error
   state; no Telegram PhotoData/DocumentData identities.
4. ViewActions: explicit copy/search/image-preview/file/transfer handlers; no cloud
   read marker, bot commands, premium or silent network callbacks.
5. ViewInvalidation: item repaint, relayout and height changes by stable LAN ID.
   This replaces Data::Session view streams, not a fabricated Data::Session.
6. ViewServices: real lib_ui style, text, emoji, scroll and animation components.
   InputField and FlatLabel/Text::String already use actual upstream lib_ui.

A subsequent original ListWidget fork must change its constructor, owned managers,
all six data/event interfaces and selection/message-key types. It is a substantial
source refactor, not something the existing ListDelegate alone can implement.
The current application retains its honest LAN list controller while integrating
real reusable rendering and policy modules progressively.

## Newly adapted upstream logic

telegram_history_presentation.cpp ports the ordinary-message branch of
Element::computeIsAttachToPrevious (history_view_element.cpp:2391), the exact
strict abs(timestamp difference) < 900 rule, author comparison, date/unread/
service/thread barriers, previous inline markup barrier and same-topic check.
It ports recountDisplayDateInBlocks, setDisplayDate's hide override, and
previousDisplayedInBlocks skipping only hidden/empty records.

The host adds conservative guards for invalid LAN timestamps, unknown sender IDs
and inconsistent direction. Upstream Saved Messages/imported forward, ephemeral
receiver/anchor, anonymous megagroup rank, welcome context and channel-author
exceptions are NOT implemented because the LAN model does not have those modes.
These are excluded branches, not simulated Telegram state.

TelegramDateBadge adapts PaintPreparedDate (history_view_service_message.cpp:149)
and DateBadge::height; chat.style margins(10,10,10,2), padding(12,3,12,4).
QPainter/host colors and rounded fill replace upstream Painter, ChatStyle and cached
corner pixmaps. Date label localization is caller-supplied. No stock whole
ServiceMessagePainter/HistoryView widget is claimed.

## Delivery semantics that must not be fabricated

Node emits delivered only after a matching receiver ACK; it does not implement
remote read receipts. Telegram BottomInfo chooses sent/received icons using its
own unread state. Do not map LAN ACK to Telegram's read double-check. A future
status host may show Queued, Receiver acknowledged, Rejected or Unknown only
from actual outbox/ACK data. Absence from outbox alone is not proof of read status.

## Cursor-backed LAN history (current implementation)

Node::historyPage now supplies an exclusive (time,id) cursor, scoped and validated
against the actual conversation row; the page limit no longer caps total browsable
history at 1000. MainWindow preserves a message ID/vertical-offset anchor while
loading older pages. Backdated live records are revealed with at most two extra
pages automatically; more distant arrivals get an explicit older-message notice.
The controller still rebuilds loaded widgets and is not Telegram ListWidget nor a
virtualized/incremental history implementation. Search explicitly covers loaded
messages only. Same-time ordering is deterministic by stable ID, not a claim of
server-ordered sequence across peers.

## Full original-UI migration audit (2026-11-11)

The requested target is now **original Telegram Desktop UI with a LAN backend**,
not merely Telegram-like colors. The current application does not meet that full
scope. Its MainWindow, conversation controller, QListWidget host, file cards,
settings/profile/share dialogs and screen-sharing UI remain custom LAN code.
Changing those widgets' styling must not be counted as original-source migration.

### Concrete seams in the pinned source

- `mainwidget.cpp:272` constructs `Dialogs::Widget` and `HistoryWidget` with a
  concrete `Window::SessionController`; it also constructs player/changelog
  services and immediately subscribes to `Core::App().calls()`. Replacing the
  transport alone cannot instantiate this tree without cloud model services.
- `dialogs/dialogs_widget.cpp:424` derives from `AbstractSectionWidget`, takes
  `controller->session().mtp()`, constructs `Api::PeerSearch` and
  `Api::SingleMessageSearch`, then subscribes to `session().api()` loading state.
  A fork needs an explicit search/loading/navigation interface, not dummy MTP.
- `dialogs/dialogs_inner_widget.cpp:307` obtains concrete
  `session().data().chatsList()->indexed()`. Its subsequent subscriptions include
  contacts loaded, removed messages, row replacement, typing/speaking animations,
  filters and history updates. The list model is a second seam beneath the outer
  search/scroll widget; changing only `Dialogs::Widget` is insufficient.
- `history/view/history_view_list_widget.h:134` has a useful delegate, but its
  `listSource` returns `Data::MessagesSlice`, actions consume `HistoryItem`, and
  media actions consume `PhotoData`/`DocumentData`. Its constructor at cpp:563
  separately requires Main::Session for EmojiInteractions and the highlighter.
  Therefore implementing ListDelegate alone cannot produce a LAN-backed view.

### Smallest honest fork interfaces

1. **LanDialogsSource**: immutable rows keyed by LAN conversation ID, sorted
   visible IDs, preview/real delivery state/unread count, local contact/group
   metadata, and insert/remove/change notifications. Adapt actual original
   InnerWidget geometry, row hit testing, selection and paint traversal to these
   rows. Keep current source-backed row renderer; do not invent History objects.
2. **LanDialogsSearch**: local query + loading/error/result producers. Separate
   this from roster discovery so search does not pretend to be global Telegram
   search. Paging search currently covers loaded messages only.
3. **LanNavigation**: active conversation producer, open/back callbacks, responsive
   column state and local capability flags. Replace SessionController references
   within the fork, rather than recreating a partial cloud controller.
4. **LanHistorySource / LanHistoryItem**: stable IDs and cursor snapshots already
   supplied by Node; explicit update streams, message presentation and height
   invalidation are still needed for an original virtualized ListWidget fork.
   Replace all concrete HistoryItem/FullMsgId/Data::MessagesSlice dependencies
   consistently, including selection/highlight/media actions. Session-owned
   emoji/reactions/translation managers must be optional behind capabilities.
5. **LanViewServices**: actual upstream lib_ui styles/text/emoji/widgets plus
   local image/file resolvers and explicit open/copy/save actions. Disable cloud
   premium/bot/story/call features rather than provide fake successful services.

Implement in a source-preserving fork, retaining license headers and a per-file
change ledger. First acceptance target: original dialogs traversal and original
history scroll/selection driven by two real LAN peers, with no MTProto/network
stub and no Telegram account/session. Check actual sending, replay/restart,
long-message folding/copy, paging anchor, file cards and screen-sharing access.
Only after this passes should the custom controller be retired. There is not yet
an implementation or passing test of those complete original forks.

### Immediately reusable containers (smaller but real progress)

`Telegram/lib_ui/ui/widgets/scroll_area.h` defines `Ui::ScrollArea` as
`RpWidgetBase<QScrollArea>`, with no Main::Session requirement. It supports existing
viewport/verticalScrollBar APIs plus original styled scrollbars, touch scrolling,
`scrollTopChanges` and owned-widget lifecycle. The FULL library already builds its
implementation. MainWindow's `QScrollArea*` can hold this real component while
preserving the LAN history controller. Prefer `setOwnedWidget(object_ptr<...>)`
when handing over the canvas. Verify resizing, anchor retention, wheel and touch
behavior rather than infer correctness from a screenshot.

`Ui::RpWidget` and `Ui::VerticalLayout` likewise provide actual source-backed
geometry/lifetime machinery without cloud sessions. Adopting them is a valid
incremental migration, but **not** migration of Dialogs::Widget, HistoryView or
MainWidget. Existing original InputField and FlatLabel/Text::String are already
real components; their surrounding toolbars and conversation logic are custom.
