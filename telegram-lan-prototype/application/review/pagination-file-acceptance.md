# Pagination and file timeline acceptance

Fresh verification passed: FULL 17/17 groups (42.41s), default 17/17 (36.30s),
FULL UI 125/150/200% 22 reported passes each. See verification-latest logs.
The initial FULL run exposed a test baseline error (12 existing unread + offer);
fixed the test to assert baseline+1 without clearing unread, then rebuilt/reran.
The following requirements are covered by the added and existing tests; they do
not imply Windows or full Telegram feature parity.

## Required history behavior

- Stable cursor ordering, including equal message timestamps and inserted rows.
- No loss or duplication across pages; retain original stable message IDs.
- Earlier-history action preserves the reader's viewport instead of jumping down.
- Switching conversations invalidates the previous conversation's pagination state.
- Long original text, author, image, delivery receipt and local file event metadata
  survive the same query path. A missing legacy receipt stays Unknown.
- Loading the oldest page terminates without repeatedly offering the same page.

## Required file behavior

- Cards derive state and byte counts from the real transfer service, not text or
  a timer. Failed/canceled cards keep their last known byte progress.
- Local timeline history does not create a second network chat message or change
  the wire protocol; duplicate transfer events do not duplicate timeline entries.
- Completion means the actual validated file was published; offering or hashing
  is not completion. Missing files and interrupted/restarted sessions are explicit.
- Open action only for an actual completed existing file; never auto-open incoming
  content. Offer accept/reject and transfer pause/resume use existing service rules.
- Sender/receiver destruction, modal confirmation, duplicate download, canceled
  hashing and path validation regressions remain covered.

## Whole-app verification

Use full-ui-probe/verify-integrated.sh after collaborators finish edits. It builds
FULL and default serially, runs both CTest suites and FULL UI at 125/150/200%.
Only a fresh PASSED marker means every requested command succeeded in that run.
The script needs the existing Linux dependencies; it is not a portable bootstrap.

Windows remains unverified: native dependency port, build/package, IME, tray,
FAT/exFAT completion and the user's original recipient crash require Windows
acceptance. Linux loopback tests are not a substitute for real dual-PC LAN tests.

## Implementation provenance and protocol limits

The new file cards, local transfer-history mapping and cursor API are LanLink
integration code, not copied Telegram HistoryView/DocumentMedia components.
They coexist with the previously attributed genuine Telegram components.
Sender-side group files retain the local group conversation context and display
each destination device separately. Versioned optional group context now routes upgraded receivers to the matching
group, including a first-file TOFU declaration. No-context offers remain direct;
older receivers may show group files as direct files, and lack of confirmation
is visible on the sender card. This is not authenticated group membership.
Sender state `sent` only proves sending completed, not receiver publication or
human read. Receiver `completed` requires validation and final file publication.
