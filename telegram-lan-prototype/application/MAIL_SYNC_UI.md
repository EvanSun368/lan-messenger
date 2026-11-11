# Mail sync and UI: implemented scope and limits

This describes the current independent mail module, not a production email-client
acceptance claim. Mail account identity and state are separate from LAN discovery,
chat identity, file transfers and Telegram session state.

## Account setup and polling

Automatic sync is enabled separately for each account. The account dialog exposes
a checked-by-default opt-in checkbox before configuration is saved; existing
accounts retain their selection. The main window can enable or disable each
account independently. The selected polling interval is **shared across enabled
accounts**, not a separate interval per account: 1, 2, 5 or 15 minutes, defaulting
to 2 minutes. Enablement, interval and notification preference are persisted.

Polling requires an available credential backend and saved usable credentials.
The timer never opens a browser or requests interactive authorization. OAuth
browser authorization is an explicit action; background operations may refresh a
saved refresh token. Accounts without usable credentials are skipped. Operations
are serialized, defer to modal dialogs and foreground work, and background
failures apply bounded retry backoff up to 15 minutes. Polling is not IMAP IDLE or
push delivery, and these intervals are not guarantees of exact delivery latency.

## Inbox identity and new-mail detection

IMAP uses read-only EXAMINE followed by UID SEARCH ALL. A valid nonzero uint32
UIDVALIDITY is required before searching or fetching. The account cursor consists
of account ID, UIDVALIDITY and a high-water UID. A first successful list establishes
a silent baseline, including when the inbox is empty. Later UIDs above the saved
high-water mark are deduplicated and treated as new. Reordering and deletions do
not lower that mark or generate repeated alerts.

A UIDVALIDITY change clears the old inbox content and establishes another silent
baseline. Body and header fetches carry the expected UIDVALIDITY and stop before
UID FETCH when it differs from the current mailbox. Returned UID and mailbox
metadata are checked before content is displayed. Changing the login identity or
server requires a new account rather than silently reusing its cursor or secrets.

Cursor storage is bounded to 1,024 accounts, 100,000 UIDs per input result and a
1 MiB state file. Persisted cursor data contains identifiers only, not headers or
bodies. Invalid input does not advance the cursor; invalid state-file loading does
not partially replace valid in-memory state. Saves use atomic file replacement.
Unread badges describe newly observed mail during this app session, not a full
mirror of the server's Seen/Unseen flags. Badges are not persisted as unread state.

## Bounded previews and account isolation

The inbox displays the latest 500 UIDs in descending order. It enriches up to the
latest 30 entries with missing summaries by fetching only these header fields:
From, To, Subject, Date, Message-ID, Reply-To, References and In-Reply-To. Each
header literal is limited to 64 KiB. This is not an automatic full-body download.

A user explicitly opens a message to retrieve its body with BODY.PEEK[]. The
transport limits full messages to 32 MiB; MIME parsing also applies its own limits.
Messages are rendered as safe plain text without loading external resources.
Attachments are saved only through an explicit file-selection action and are not
automatically opened or executed. There is no offline body cache, full mailbox
archive, folder management, delete/move UI or server flag synchronization.

Inbox summaries, unread markers and draft buffers are keyed by account. A
background result stays associated with the operation's account even if the user
selects a different account. Foreground reading verifies both account and mailbox
identity before showing a body or attachments. Switching accounts clears the
visible received message and restores that account's in-memory compose draft.

Drafts, message bodies and summary headers are not written to a mail-content
cache. Account configuration, sync preferences and cursor IDs are saved locally;
secrets use the system credential backend. Explicitly saved attachments are an
intentional exception to the no-content-cache policy. Draft attachment choices
refer to local files and are rechecked when preparing a send.

## Closing, cancellation and sending

Closing the mail pane hides it, cancels the current mail operation and clears all
in-memory account drafts and the displayed message after any required confirmation.
Previously enabled polling continues while the application remains running.
Application shutdown is a separate boundary: shutdown/aboutToQuit stops the timer
and cancels active OAuth and transport work. Destroying the window also cancels
its operations. This is not a background service that survives application exit.

Reply creates an editable draft using validated reply metadata. It does not send.
Replacing an existing compose draft requires a user choice. SMTP submission needs
an explicit send and confirmation showing account, recipients, subject and
attachments, with message text available for review. Sending is never triggered
by a poll, a reply action, a chat export or a notification.

A successful SMTP final acknowledgement means server acceptance, not delivery or
read receipt. The draft remains available after sending. If the connection fails
after body submission without a final acknowledgement, the result is labelled
unknown. Cancellation cannot undo an already accepted message. The implementation
does not automatically resend; the user must check the outcome before retrying.
There is no server Drafts/Sent APPEND integration.

## Notifications and privacy

New-mail notification text contains only the app name and the new-message count,
not sender, subject, account address or body. Notifications can be disabled while
sync remains enabled. The UI also maintains per-account badges. The initial and
UIDVALIDITY-reset baselines do not notify for historical mail. Tray notification
clicks can reopen the mail pane and select the associated account.

Actual operating-system tray availability, delivery, permission behavior and
notification-click routing require native runtime validation. A mock signal or
headless Qt test is not evidence that desktop notifications were delivered.

## Telegram UI reuse and platform gap

In the FULL build, the mail UI uses original Telegram UI components through the
project's adapters, including its dialog-row presentation and input/action
controls. The mail controller, account/settings dialogs, compose/inbox organization
and tab scaffold are custom code. This is **not an original Telegram mail
subsystem**, and it is not Telegram's account/session/network implementation.
The non-FULL build provides the corresponding standard Qt widget implementation.

The current FULL host/build integration is Linux-only, while the implemented
native credential backend is Windows Credential Manager only. The Linux backend
fails closed rather than writing passwords or OAuth tokens in plaintext. Thus the
currently exercised FULL UI and the native credential-capable platform do not yet
form a verified end-to-end product configuration. Injected mock credential stores
and mock mail transports enable local testing, not real mailbox connectivity.
Windows-native UI/build/credential acceptance, a supported Linux credential
backend, real provider interoperability and operating-system notifications remain
unverified. OAuth also requires valid provider application registration and
configuration; templates alone are not proof of provider authorization success.

## Verification boundaries

Available test sources cover account storage, OAuth, MIME, transport, sync state
and the mail-window controller in tests/test_mail*.cpp. Local fake-server tests
exercise UIDVALIDITY rejection, stale-UID prevention, header-only commands and
size bounds, TLS/STARTTLS, OAuth authentication, split binary literals,
cancellation and SMTP final-acknowledgement ambiguity. Sync-state tests cover silent
baselines, empty inboxes, rollover, duplicates, reordered/deleted UIDs, bounds and
persistence failure/corruption handling. Window tests use injected local doubles
for account isolation and interaction workflows.

One isolated Qt 6.11.2 run during this implementation passed 19 transport test
cases and 7 sync-state test cases (QtTest totals include setup/cleanup). This is
not a claim about the final aggregate build, subsequent edits, FULL integration,
Windows runtime or real providers. See the final validation record for the exact
source revision and aggregate checks actually completed. No real credentials or
external email transmission were used for these local transport/sync checks.
