# Notification and long-message transport review

Scope: actual LAN Node TCP frames, persistent outbox/ACK handling, long group
messages and notification counter integration. No Telegram cloud transport is
used. Windows system-tray/toast appearance is not validated by Linux offscreen
tests.

Reviewed implementation:
- queue validates UTF-8 size (128 KiB), advertised recipient capabilities and
  total serialized 1 MiB frame before committing outbox/history transaction.
- flush retains retries for transport errors; only matching message ACK removes
  the recipient row. Certain permanent protocol rejection errors stop retry.
- receiveChat commits message before ACK; duplicate message IDs return ACK
  without re-emitting message (therefore do not double-count unread).
- Group delivery uses one recipient outbox row per member. Delivery succeeds
  independently per member; there is no central always-online server.
- Desktop notifications can be disabled without disabling unread accounting.
  Notification text excludes message body. Counters currently live in memory,
  so app restart does not restore pre-existing unread counts from history.

New tests in test_notification_transport.cpp:
- 110000-byte UTF-8 group message delivered to two real loopback Nodes;
  repeat identical group request receives ACK, does not duplicate history/unread.
- Long offline message persisted across sender restart and delivered once,
  with one incoming notification.
- Disabled alerts retain per-conversation unread semantics.
- FULL production MainWindow: minimized inbound notification, restoration,
  narrow list view, same-selected-row re-entry, switching to another chat.

Detected bug in production window integration: after showNormal+activateWindow,
the window is active/non-minimized and conversation visible, but unread remained
1 because only WindowActivate was observed. Parent notified with concrete
deferred Show/WindowStateChange visibility-check fix. Results after fix should
be recorded separately; no passing claim for this case until rerun.

Verified after the parent applied deferred activation/visibility checking:
FULL Qt 6.11.2 test_notification_transport: 6/6 passed (4 functional cases plus
Qt setup/cleanup), 1693 ms. This includes the reproduced restoration regression
and single-click reopening of the same selected conversation. Linux offscreen
logs contain expected unsupported tray/raise/cache-directory warnings; no Windows
native notification claims are made.
OFF Qt 6.8.3: 5/5 passed (3 functional cases plus setup/cleanup), 273 ms.

Follow-up nondeterministic test diagnosis: final programmatic selection occurred
while the native window was inactive. Five diagnostic runs confirmed inactive,
non-minimized, visible chat with correct Carol ID/title. Keeping Bob unread was
correct behavior, not evidence of a transport failure. The test now explicitly
checks background programmatic Bob selection retains unread=2, then foregrounds
the window and performs a real list mouse click. It asserts active state, list
ID, conversation title and visibility before requiring unread=0. Two consecutive
five-run batches passed all six cases. Parent also deferred/coalesced roster
refresh to avoid synchronous item deletion during selection; this is preventive,
not the diagnosed cause of the inactive-window assertion.
