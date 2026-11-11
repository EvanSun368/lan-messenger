# Telegram conversation-list integration component

Upstream: https://github.com/telegramdesktop/tdesktop
Pinned commit: c4bd064bcc84a359179e191fa6dfd216acda504f (7.4.1)

## Reused implementation

`telegram_dialogs_model.h` actually adapts these upstream implementations:

- `Telegram/SourceFiles/dialogs/dialogs_list.cpp`:
  `List::adjustByDate`, `List::adjustByName`, `List::rotate`,
  `List::rowAtY`/`findByY`, and index/top maintenance after height/removal.
- `Telegram/SourceFiles/ui/unread_counter_format.cpp`:
  `FormatUnreadCounter` and `FormatUnreadCounterShort`.

The algorithms were isolated from Telegram `Key`, `Entry`, `Row`, `Session`,
`not_null` and ranges dependencies. LAN QString IDs and simple row structures
replace account/cloud objects. Name sorting is case-insensitive. Frozen lists,
pinned items, folders, premium UI, sponsored content and Telegram account logic
are deliberately excluded. The original upstream GPLv3-or-later and OpenSSL
exception apply; the project's included LICENSE/LEGAL must accompany this code.

## Actual RowPainter subset now adapted

`telegram_row_painter.cpp/.h` additionally adapts the backend-neutral drawing
path from `dialogs/ui/dialogs_layout.cpp` (`PaintRow`, `PaintRowTopRight`,
`PaintWideCounter`) and `ui/unread_badge_paint.cpp` (`PaintUnreadBadge`).
It retains background/avatar/name/date/preview-counter ordering, date-width
reservation before elision, counter width subtraction and badge text placement.
Geometry (62px row, 46px avatar, 10/8 padding, name x68/y10, preview y34) comes
from `dialogs/dialogs.style`; base colours come from `lib_ui/ui/colors.palette`.

Session-backed objects are replaced by `DialogPresentation`, an immutable value
snapshot. Qt font metrics and QPainter replace lib_ui wrapper methods. Avatar
clipping/fallback and rounded-badge painting are new adapters rather than cached
Telegram userpic/circle-half code. Fonts are pixel-sized to retain upstream logical
geometry; font family is left to the integrating application.

`conversation_list.cpp/.h` is a new scroll container that now invokes this derived
basic row renderer. It is still NOT the complete original Telegram RowPainter:
rich-text entities, video avatars, animated custom emoji, premium/verified icons,
cloud drafts, support mode, topics, service previews, swipe actions and ripple
animations remain excluded. The exclusion is explicit instead of faking a Session.

Existing LanLink QListWidget UI can directly use header-only `DialogsModel`
without adopting the demonstration QWidget wrapper:

```
TelegramLan::DialogsModel rows(TelegramLan::DialogsModel::SortMode::Date);
rows.upsert({id, name, preview, timeText, sortKey, unread});
for (auto row : rows.rows()) { /* populate QListWidget with row->id/name */ }
```

The model maintains ownership independently. Do not retain returned row pointers
after removing that ID or destroying the model. Selection should be retained by
stable ID in the consuming UI.

## Verified

Qt 6.8.3 / GCC14.2, Linux offscreen: 7 functional tests passed (9 including Qt
init/cleanup): date movement both directions, name movement, variable-height hit
testing/removal, unread formatting, UI mouse selection after filtering, source row geometry/palette and narrow counters.
No Windows runtime validation or upstream complete application build was done.
