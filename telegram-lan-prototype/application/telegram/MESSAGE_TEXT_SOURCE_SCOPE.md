# Actual Telegram message text engine integration

Pinned lib_ui: 42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4.
Original linked sources, unchanged: ui/widgets/labels.cpp/.h and ui/text/*,
including text parser, layout, emoji rendering, selection and copy.
GPL-3.0-or-later with upstream exception; see DESKTOP_APP_LEGAL and LICENSE.

TelegramMessageText is NEW hosting glue, not an upstream history widget. It uses
real Ui::FlatLabel, whose Ui::Text::String engine also renders upstream
history/view/history_view_message.cpp (text().draw at line 3173 of pinned
Telegram Desktop c4bd064bcc84a359179e191fa6dfd216acda504f). No QTextBrowser is used
inside this component. Runtime/style/emoji initialization comes from the existing
full-source InputField integration.

Full HistoryView::Message/ListWidget remain unported: they reference Main::Session,
History, cloud reactions/drafts, API and MTProto services. These dependencies are
not simulated. Existing LAN message rows, image viewer, history data and explicit
link-preview confirmation controls remain in the application.

## Long-message fidelity

Original text_block_parser.cpp limits one string to 0x8000 UTF-16 units. The host
splits rendering at <=16000 units, normally at grapheme boundaries, into real
original labels. Extremely long single graphemes fall back to safe codepoint
boundaries. Original complete text is retained for copying and transmission.
Each slice supports original text selection; cross-slice drag selection is not
implemented. Each slice has Copy whole message; Ctrl+C on the container or an
unselected label copies the entire text. Slices add no layout spacing but use
separate text lines.

Plain text is not made into active links. External navigation cannot bypass the
existing confirmation controls. Custom stickers, animated emoji, rich entities,
markdown, Telegram link handlers and premium decorations are not claimed.

## Verified component tests

Qt6.11.2 full mode: 6 functional cases passed (Qt total 8 with init/cleanup).
Checks include >128KiB Chinese/emoji, 131072 ASCII characters, family-emoji and
combining text. Parsed accessibility content is checked per source label to catch
silent upstream truncation. Whole-message and unselected-child Ctrl+C copying,
original selected-word copying, and font-responsive layout pass. This component
suite does not itself prove the application has connected the new renderer.

The host sets FlatLabel style minWidth=1 and maxHeight=0. Original default
minWidth=0 is interpreted as kQFixedMax by FlatLabel, which prevents wrapping;
this was caught by integrated height tests and corrected in host configuration.
Regression tests now verify real long-text label height, actual ink in the last
40 rendered pixels, and increased height after narrowing the allowed width.
Original source is unchanged. Same-width updates return immediately to avoid
viewport relayout loops.

## Optional long-message presentation

The LAN host now provides setCollapseLongMessages(true), disabled by default for
source component callers. It clips a viewport to about 12 visual line heights and
adds an explicit Expand whole message / Collapse button. All original labels keep
their full source text and full layout height; neither transport/history text nor
parser strings are shortened. Expanded/collapsed state survives font and width
changes. This folding control is new LAN UI glue, not claimed as original Telegram
history-widget code. Source-engine long-text and bottom-pixel tests remain enabled
with folding disabled/expanded.

Both source-label and host context menus offer whole-message copy and atomic UTF-8
text saving. Existing history search continues to use unchanged complete text;
the folding viewport does not alter the underlying search or storage model.
