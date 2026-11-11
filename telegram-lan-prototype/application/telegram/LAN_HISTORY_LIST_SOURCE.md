# Actual ordinary-list source fork and transitional app adapter

Source: Telegram Desktop c4bd064bcc84a359179e191fa6dfd216acda504f,
GPL-3.0-or-later with upstream exception; original LEGAL/LICENSE retained.

`telegram_lan_history_list.{h,cpp}` explicitly forks ordinary-message mechanics
from `Telegram/SourceFiles/history/view/history_view_list_widget.cpp`:
- resizeGetHeight at 3256: setY / pendingResize / height accumulation loop.
- paintEvent at 3486: lower_bound visible-range selection, translated item draw.
- findItemIndexByY/strictFindItemByY at 4049: binary search and endpoint clamp.
- countScrollState at 4091 and restoreScrollPosition at 3310: stable item plus
  pixel offset. LAN QString id replaces Data::MessagePosition/HistoryItem.

Intentional exclusions: Main::Session, MTProto, sponsored items, cloud about-view,
collapse-gap/reveal animation, emoji effects, reactions, remote read marking,
complex text/multimessage selection and cloud media managers. No fake Session is
created, no cloud methods are stubbed to return success. The previous generic
bubble module is not repackaged as this new contribution.

LanHistoryLayout is a NEW QVBoxLayout-compatible bridge. It preserves actual
QLayoutItem ownership and count/itemAt/takeAt/insertWidget/addStretch calls, but
its setGeometry does not call QVBoxLayout's geometry engine. Instead it creates
explicit LAN HistoryElement adapters and invokes the source-derived ordinary
list layout, which assigns the real message widgets' geometries. The existing
terminal stretch is excluded from message geometry; current app spacing is zero.
Margins are host geometry, not Telegram cloud state.

This is NOT complete stock ListWidget or Element. Existing message text, dates,
file cards and controls are QWidget children; their original interactions remain.
WidgetElement::draw is deliberately empty because Qt paints those children once.
The source-derived paint loop is provided for the next true painted Element
migration, but that loop is not claimed as the app's child painting path. All
loaded child widgets remain allocated: no memory virtualization is claimed.
Visible-range hit testing and stable-id anchors are available without hiding
widgets (which would invalidate Qt layout measurements).

The standalone element interface allows testing paint clipping without inventing
Telegram HistoryItem. The FULL application now constructs LanHistoryList as the message canvas and
LanHistoryLayout as m_messageLayout, including actual text/image/file rows.
Scroll changes update its visible range. Source/type assertions and real history
paging/long-message application regressions cover this live path; final run
results are recorded in VALIDATION.md, not inferred merely from these files.

## Short-history bottom alignment

LanHistoryLayout now adapts ListWidget::countItemsTop (6036) and
countBottomPadding (6029): available minimum height minus content plus bottom
padding, floored at zero. Host contentsMargins are kept outside the ordinary
message region; original chat.style uses historyPaddingBottom=8, while callers
may retain their existing explicit margin. No cloud about-view or collapse gaps
are simulated. Short histories finish above bottom padding; overflow starts at
the top margin. sizeHint/heightForWidth report actual content, not viewport filler,
so there is no artificial scroll range. Stable-ID anchors include the new origin;
prepending to a still-short list leaves existing rows stationary, while prepending
to overflow history increases their coordinates by the actual inserted height.
