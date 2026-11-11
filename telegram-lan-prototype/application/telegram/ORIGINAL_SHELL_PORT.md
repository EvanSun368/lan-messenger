# Original-control shell integration

Goal: use real upstream Telegram/Desktop App Toolkit widgets with the LAN data
model, not claim that a Qt stylesheet is a Telegram Desktop port.
Pinned Telegram Desktop source: c4bd064bcc84a359179e191fa6dfd216acda504f;
lib_ui and other submodule revisions remain in full-ui-probe/SUBMODULES.txt.
Upstream source is unmodified. The GPL/LEGAL files retain their original terms.

## Real upstream implementations used in FULL mode

- Ui::Platform::DefaultTitleWidget: title painting, system control buttons and
  title interaction. Host helper: telegram_original_title.h.
- Ui::IconButton: navigation, back, search, attachment, send and more actions.
- Ui::PopupMenu: actual menu painting, keyboard/navigation and submenus. Existing
  LAN QAction callbacks are bridged rather than replaced by cloud actions.
- Ui::InputField (SingleLine) plus Ui::CrossButton: both search fields, including
  actual editing and clear-button implementation; host API in search_field header.
- Ui::ScrollArea: message scrolling container and owned content widget.
- Ui::InputField: existing original compose field, IME/editor/font logic retained.
- Ui::FlatLabel/Text::String: existing original message text rendering retained.
- Ui::RpWidget: central window host. This alone does not imply an original app shell.

lan_original_dialogs.style is compiled with the ORIGINAL codegen_style tool.
It extracts dialogsMenuToggle/dialogsFilter from SourceFiles/dialogs/dialogs.style,
topBarSearch from SourceFiles/info/info.style, and historyAttach/historySend.inner
visual data from SourceFiles/chat_helpers/chat_helpers.style. Exported names have
an explicit LAN prefix. Original PNG masks at Resources/icons are consumed by
codegen; they are not redrawn placeholders. Search/send/attach geometry and colors
come from those declarations. Send uses original IconButton plus SendButton.inner
visuals, NOT the complete SendButton with paid-message, voice and schedule modes.
Bubble fill uses actual upstream st::msgInBg/msgOutBg in FULL mode.

## Still custom or adapted; NOT complete Telegram Desktop UI

MainWindow controller, Qt layout/splitters, QListWidget host and LAN conversation
model remain custom. Row painting, bubble geometry and grouping are documented
source adaptations, not a complete Dialogs::Widget or HistoryView::ListWidget.
The header text host, file cards, settings/profile/sharing/transfer/screen dialogs,
emoji picker UI and some auxiliary controls remain custom. The LAN actions and
permissions necessarily differ from Telegram's cloud account/session services.

A complete original Dialogs/History/MainWidget fork requires explicit replacement
of their Session, HistoryItem, indexed chat list, search/API and event interfaces.
See HISTORY_SESSION_BOUNDARY.md; copying only their transport calls or creating a
fake Session does not satisfy this requirement. This stage does not pretend it does.

Default mode still uses lightweight Qt adapters. Both modes in this restored
workspace use Qt 6.11.2. A default screenshot is not evidence of FULL original
widget behavior. Screenshot/test evidence must be generated after this build,
not inherited from the prior source lab or from before the environment reset.

## Integration defects caught and fixed

Original IconButton initializes a size but does not expose the Qt layout minimum
used by this host. Explicit original style dimensions now prevent Qt layouts
collapsing the search/attach/send paint area to zero width.

Original PopupMenu(QMenu*) takes ownership through Ui::Menu. Passing a persistent
LAN command menu previously destroyed its model on close. Every popup now receives
an independent recursive command copy, with QPointer-guarded forwarding to the
persistent actions. Repeated opens of all three menus are exercised by UI tests.

These are host integration corrections, not changes to upstream implementations.

## Fresh verification

Qt6.11.2 FULL:17/17 CTest groups (53.20s); default:17/17 (41.30s).
FULL UI at125/150/200%:23 passes each. See verification-latest logs and PASSED.
The actual screenshots in application/screenshots/original-shell-* use demo data.
No new shell ASAN run, native Windows test or complete Telegram parity is claimed.

## Subsequent ordinary-list and settings stage

The following source-ledger documents supersede this shell-stage inventory:
DIALOGS_INNER_SOURCE_SCOPE.md and LAN_HISTORY_LIST_SOURCE.md. The visible list is
now a source-derived RpWidget ordinary-list fork rather than QListWidget, the
history canvas uses original layout mechanics through explicit LAN Elements, and
settings use original GenericBox/LayerManager. The hidden Qt list remains storage,
and QWidget history elements, other auxiliary dialogs and cloud-model exclusions
remain explicit limitations. Final validation is tracked in ../VALIDATION.md.
