# Ordinary-dialog list fork

Upstream Telegram Desktop source pinned at c4bd064bcc84a359179e191fa6dfd216acda504f.
Source: `Telegram/SourceFiles/dialogs/dialogs_inner_widget.cpp`, GPL-3.0-or-later with the upstream exception (see upstream LEGAL/LICENSE).

## Actual production branch transferred

`telegram_dialogs_inner.cpp` transfers these ordinary-list branches into a `Ui::RpWidget` owned by the real `Ui::ScrollArea`:
- `paintEvent`, lines 1322–1350: find first clipped row, translate to its top, paint until clip bottom, advance by each row height. Fixed-height LAN index replaces `List::findByY` and Entry-backed row geometry. Pinned drag/collapsed folders are removed.
- `mousePressEvent` and `mousePressReleased`, lines 2494ff and 3031–3142: retain selected/pressed/button separation and require the same valid row at left-button press/release before activation. Qt persistent indices replace Entry keys. Removal during press invalidates activation.
- `selectSkip`, lines 5495ff: first entry when no keyboard selection, otherwise clamp current+direction. Keyboard dispatch follows the original Up/Down/Page/Home/End/Enter branch. Fixed-height pagination replaces variable-height/cloud sections.

The rendering path calls the existing explicitly adapted PaintRow geometry/avatar/unread painter with text disabled, then uses unmodified **Ui::Text::String::drawElided** for title and preview. Its real parser/emoji/bidi machinery is in production, not an unused demonstration. The row geometry/background/avatar/counter adapter remains modified Qt drawing, not full unmodified RowPainter.

## LAN data boundary and integration

`DialogsView` consumes `QAbstractItemModel`, `QItemSelectionModel` and a presentation callback returning `optional<DialogPresentation>`. The persistent Qt model is only storage. Main's legacy QListWidget must be hidden/nonfocusable; the visible widget, scrolling, pointer/key dispatch and activation are this view. `nullopt` excludes filtered rows; `refresh` handles host-only itemHidden changes. `activated` opens current conversation including repeat activation. `rowRect` exposes viewport coordinates for actual input tests.

No fake Telegram Session, authentication, PeerData or cloud History is constructed. `InnerWidget` construction, IndexedList/Entry ownership and RowPainter cannot be compiled unchanged against a different transport: they directly depend on SessionController, Data::Session, History, cloud filters, forum/topic/collapsed state and MTProto search. These branches are removed, not emulated. This is a source-derived ordinary-row fork, **not the whole original Dialogs::InnerWidget**.

## Regression scope

`test_dialogs_inner` exercises actual pointer activation, different-row release cancellation, row removal while pressed, keyboard navigation, scroll-to-selection and host filtering. Parent integrates production view and full application tests separately. Do not claim tests passed before execution.

Name text uses the exact `st::semiboldTextStyle` from `dialogs_entry.cpp::chatListNameText` (line335); preview uses `st::defaultTextStyle`, aliased by upstream `dialogs.style::dialogsTextStyle`. Keyboard/mouse arbitration also retains upstream `mouseMoveEvent` same-global-position guard (line2065), preventing scroll-generated synthetic moves from replacing keyboard selection.

Qt-host adjustment: on keyboard navigation the cursor baseline is sampled from `QCursor::pos()` before `ensureVisible`. QScrollArea synthesizes a mouse move using that platform coordinate; event replay/touch coordinates can differ from the last mouse press. This prevents a stationary pointer from replacing End/Home selection during scrolling while still permitting a subsequent actual pointer move. The regression retains the End→Enter last-row assertion (row59), plus Home, wheel, filtering and removal-during-press; targeted FULL test passed four consecutive runs on the fix.
