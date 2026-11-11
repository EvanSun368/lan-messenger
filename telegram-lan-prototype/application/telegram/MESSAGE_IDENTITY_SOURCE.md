# Ordinary Message presentation source port

Pinned Telegram Desktop c4bd064bcc84a359179e191fa6dfd216acda504f.
GPL-3.0-or-later and upstream exception; retained project LICENSE/LEGAL apply.

`PresentHistoryMessage` directly adapts ordinary Context::History and default
chat mode branches from `history/view/history_view_message.cpp`:
- hasFromPhoto (3916), displayFromPhoto (3397): incoming non-user conversations
  reserve a photo column; display only when not attached to the next message.
- hasFromName (5816), displayFromName (5864): incoming group name, unless media
  hides it or the message is attached to its predecessor.
- countMessageRounding (6637), countBubbleRounding (6675): directional small
  corners for attached bubbles, tail on the last ordinary bubble; media/inline
  keyboard tail suppression and keyboard bottom small corners.
- `history_view_element.cpp` setAttachToPrevious/Next (2794–2835): bubble attachment
  additionally requires the adjacent element to be wrapped. It is not equivalent
  to grouping attachment, so HistoryDecoration carries both sets of flags.

LAN mapping: outgoing is the stored direction; groupConversation is an actual
local group conversation, not a manufactured PeerData object. One-to-one messages
show neither author names nor photos; outgoing group messages also show neither.
Incoming group name is at group start and photo at group end, with photo space
reserved across the group. The caller must apply both display and reserve flags.

Excluded: cloud channels/posts, Saved Messages/imported forwards, verification
code peers, guest bots, admin logs, stories, PSA, rich article media and special
wide-chat modes. Hidden/empty/service/sponsored entries receive conservative
no-author-display handling. No claim is made that these exclusions reproduce
those special Telegram UI contexts. LAN callers must not set unsupported fake
cloud flags to pretend they are available.

This module supplies real source-derived presentation decisions. Rendering a
Tail still requires actual tail support in the caller's bubble painter; merely
passing the enum does not establish rendered visual equivalence. Whole original
Message/Element classes and their cloud data models have not been transplanted.

## Actual tail rendering integration

`bubble.h` now executes the original PaintSolidBubble tail branch from
`ui/chat/message_bubble.cpp:271-297`: choose left/right icon, calculate tailShift,
paint at bottomPosition-tailShift, return the real tail width. The four icons are
renamed exports of chat_style.style (216–219), using the pinned original
Resources/icons/bubble_tail PNG masks and codegen horizontal flip. They are not
redrawn approximations. TelegramBubbleFrame::setRounding applies all four actual
presentation corners. The host reserves icon width inside its widget to prevent
clipping; this reservation is a Qt host adaptation.

FULL radii follow chat.style (large16 / small roundRadiusLarge6). Corner ellipse
rendering remains the prior custom painter, not the whole original ChatStyle or
cached corner pixmap pipeline. Lightweight builds without original icon runtime
explicitly fall back to Small corners. Original tail pixel tests compare exact
mask output at DPR1/2 and verify actual frame painting changes with attachment.

The host now reserves the outside tail column for **every** bubble of the same
incoming/outgoing direction, including attached members without a tail. This is
a host QWidget geometry adaptation preventing body edges shifting on the final
message. `TelegramBubbleFrame::tailInset()` exposes the original icon width;
`layoutPadding()` adds that outside inset to original msgPadding (11,8,11,8).
Only the actual Tail rounding paints the mask. Mid-body scanline tests compare
attached and final rows and verify content padding remains unchanged.
