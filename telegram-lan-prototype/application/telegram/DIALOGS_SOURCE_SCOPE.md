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

## Actual row drawing migration

`telegram_row_painter.h/.cpp` adapts the backend-neutral drawing subset from
`Telegram/SourceFiles/dialogs/ui/dialogs_layout.cpp` (`PaintRow`,
`PaintRowTopRight`, `PaintWideCounter`) and
`Telegram/SourceFiles/ui/unread_badge_paint.cpp`.
Default geometry comes from `dialogs/dialogs.style`; palette values come from
desktop-app/lib_ui `ui/colors.palette`.

Actual retained behavior: background/avatar/name/preview drawing sequence,
date-width reservation before title elision, unread badge sizing and width
reservation, narrow avatar-only badge layout, and default 62px rows/46px avatars.
QPainter/QFontMetrics replace lib_ui painter/style wrappers; circular avatar
fallback and rounded badge drawing replace Telegram's session-backed userpic
and cached badge image halves. These replacements are new adapter code.

`src/mainwindow.cpp::RosterDelegate` now calls this renderer for all three
existing tabs (messages, contacts, groups). The message tab supplies preview and
time; contacts/groups supply LAN address/member details without message time.
Unread counts, stable ID selection, searching, click/open behavior and the
existing data model remain in LanLink. Painting consumes a snapshot only, with
no session/network lookup.

This is NOT the complete Telegram RowPainter or complete Telegram widget.
Rich text, custom emoji/video avatars, cloud drafts, premium/verified badges,
topics, support state and swipe/ripple remain excluded. The full widget's
Session/History subscriptions have not been migrated.

## Validation

Standalone renderer/model tests: 7 functional cases (Qt total 9), including
source layout coordinates, non-overlap, narrow counters and sorting/hit testing.
Production delegate test `telegramRowPainterIsActuallyConnected` checks the
actual renderer height and selected-row palette in all 3 tabs. Existing UI
integration tests continue to exercise search, selection, drafts and notifications.
Windows runtime validation is not claimed.

Linux Qt 6.8.3 integrated application validation after renderer integration:
7/7 CTest suites passed (25.73s); updated the previous 78px-card size assertion
to the migrated 62px source style. Demo screenshots in
`screenshots/telegram-row-{sessions,contacts,groups}.png` were captured from the
running application and visually inspected. They are demo data, not remote peers.
