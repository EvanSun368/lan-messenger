# Telegram UI specification acceptance matrix (work in progress)

Reference supplied by owner thread: Notion page
https://app.notion.com/p/4143df90414581a7b7c2e806fbda0cce
"局域网聊天工具 · Telegram Desktop 功能与 UI 规范 v0.1", edited 2026-11-10.
This is an assessment of the source lab, not a claim that the specification is done.

| Requirement | Verified / implemented | Remaining |
|---|---|---|
| Genuine Telegram source | Original full InputField, FlatLabel/Text::String, emoji engine; source-derived columns/rows/bubbles/history grouping | Full HistoryView/ListWidget Session refactor not done |
| Navigation | Single-window list/chat, narrow back, messages/contacts/groups/transfer/settings existing routes | Dedicated 60px navigation, exact visual tokens and saved window geometry not complete |
| Lists | Name/preview/time/unread; shared conversation identity; source row rendering | Pin/mute/drafts in list; clear availability labels; dynamic enlarged-font metrics |
| Input/IME | Simulated preedit, draft switch, clear/focus cancel, real loopback send, font/emoji | Native Windows IME and draft cursor/reply-target persistence |
| Long messages | UTF-8 128KiB boundary; rejected draft intact; original renderer segmentation/full copy | 12-line fold/expand and full copy/save implemented; stable cursor pagination implemented; full-database search still not done |
| Delivery | Real matching ACK/dedup/offline queue, group long message tests | Persisted per-recipient ACK model, partial-result details and same-ID retry implemented; no remote read receipts |
| Files/sharing | Real transfer, SHA256, pause/resume, reject/cancel, request snapshots; callback/modal safety fixes | Real-state timeline cards and explicit receive/reject implemented; batch drag/drop preview, native Windows receive-crash confirmation, FAT/exFAT completion support remain |
| History | Reading position preserved in basic tests; chronological grouping/date separators | Cursor pagination and stable message anchor implemented; incremental/virtualized model and normal "new messages" jump button remain. Backdated arrivals have bounded reveal and an explicit older-message notice. |
| Notifications | Foreground/background, minimize/restore, narrow list, same-row click, disabled-alert unread behavior | Native Windows tray/toasts, persistent unread state |
| Themes/DPI | 100%,125%,150%,200% Linux offscreen UI tests pass | Native Windows mixed-DPI and dark theme not yet accepted |
| Whole app | Existing LAN services retained; temporary isolated storage in tests | Windows binary, actual two-machine LAN acceptance, complete P0 sign-off |

Unknown historical ACK state must remain Unknown. A missing outbox row is not
proof of delivery or read. Linux tests do not prove the user's original Windows
shared-file crash is fixed. User-facing final status must preserve these limits.
