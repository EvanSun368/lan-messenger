# Full Telegram RowPainter/Dialogs boundary investigation

Pinned upstream: c4bd064bcc84a359179e191fa6dfd216acda504f.
This documents the next migration, not a claim that full widgets already run.

## Concrete source boundary

`dialogs/ui/dialogs_layout.cpp:1131` (`RowPainter::Paint(Row*)`) is the data
extraction boundary. It fetches:

- `Entry::chatListBadgesState`, `chatListMessage`, `chatListNameText`, peer badge;
- `chatListPreloadData()` which may initiate Telegram message resolution;
- `History::cloudDraft(topicRootId, monoforumPeerId)` and message/draft dates;
- `History::peer`, migrateTo, self/replies/verification/saved classification;
- `thread->sendActionPainter`, `lastItemDialogsView`, forum/monoforum state;
- `entry->updateChatListEntry()` callbacks for delayed image/emoji preparation.

It calls the internal templated `PaintRow` at line 440. That function still reads
`entry->session().supportMode()`, peer verification/customEmojiManager, thread
mute state, item delivery/read state, promoted entries and avatar resources.
Therefore replacing only the public RowPainter entry argument is insufficient.

`dialogs/dialogs_inner_widget.cpp` is 7,370 lines at this commit. Its constructor
binds `_shownList` to `controller->session().data().chatsList()->indexed()` and
subscribes to downloader completion, contactsLoaded, itemRemoved, row replacements,
chat-list loading, settings, changes, filters, archive and stories state. The outer
`dialogs_widget.cpp` is 5,247 lines. Neither is a generic Qt list one can move
unchanged without the Telegram account graph.

## Executable staged plan

1. **Done:** value snapshot and isolated basic renderer. `DialogPresentation`
   supplies pre-resolved name/preview/date/avatar/unread/mute/online; renderer has
   zero network/Session access. Compile/test target `telegram_dialogs_adapter`.
2. **Integration:** existing LAN event handlers build a snapshot per stable
   conversation ID; delegate calls renderer. Keep UI selection, draft identity and
   unread acknowledgement outside paint. Never call network during paint.
3. **Foundation:** build upstream lib_ui/lib_base/lib_rpl/lib_crl plus style/emoji
   code generators. lib_ui's CMake needs external zlib/jpeg/lz4/xxhash; lib_base
   also exposes Qt/OpenSSL/crash reports/ranges/GSL/expected. Preserve original
   submodule pins, fonts licences and generated palette/style rules.
4. **Text fidelity:** replace QFontMetrics adapter with `Ui::Text::String` using
   original name/preview drawing. This needs text parser/shaper/emoji integration
   and an implementation of `Ui::Integration`; it does not need a fake Telegram
   login or Session. Test RTL, links, emoji, selection and elision.
5. **Interactions:** import `BasicRow` ripple portion and relevant lib_ui scroll
   mechanisms, then wire LAN-only context menus and unread settings. Retain newly
   isolated value model; do not recreate Telegram API classes as empty stubs.
6. **Optional state:** delivery/read indicator, local draft marker and local
   group icon map to explicit presentation fields. Telegram-specific premium,
   sponsorship, stories/topics and cloud saved-messages are removed by product
   choice, not silently represented as working.
7. **Acceptance:** compare rendered basic rows against original style at
   100/150/200% DPI, update unread/name/last-message under scroll and selection,
   change themes, verify long text/RTL and no network calls from paint. Windows
   two-machine runtime and lifecycle tests remain separate gates.

The approach keeps actual upstream drawing portions and gradually expands reuse;
it is not a whole-program Telegram fork yet. A full replacement of the original
Session/API graph should not be claimed merely because these rows render.
