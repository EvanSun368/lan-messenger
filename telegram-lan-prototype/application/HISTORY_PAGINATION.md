# LAN history pagination

New application code, not presented as an upstream Telegram pagination engine.

Node::historyPage uses an exclusive (time,id) keyset cursor scoped to a conversation.
Pages default to 200 and cap individually at 1000; total history is not capped.
The existing history() API keeps its prior bounded behavior for old callers.
A composite (peer,time,id) SQLite index supports the ordering. Existing message,
author, image and transfer-mapping data/protocol are unchanged.

Equal timestamps have a deterministic ID tie-breaker. Newer insertion cannot
shift an older page as it would with OFFSET. A cursor contains a real persisted
row key, and invalid/mismatched/nonfinite/missing cursors return explicit errors.
All query values are bound parameters. No cursor text is executed as SQL.

MainWindow offers Load earlier messages and preserves an ID + body pixel-offset
anchor, rather than assuming old scrollbar maximum stays constant. Loaded history
and expanded long-message state survive new arrivals. Initially only newest 200
messages render; each explicit click loads another page. Older pages rebuild the
current rendered window, which is not yet a virtualized list.

Live message IDs are read back from the database. If a local clock reversal puts
an arrival before the loaded boundary, up to two extra older pages can be loaded
to include it. A larger gap retains the existing window and explicitly reports
that the earlier new message is not yet displayed; the user can load older pages.
Timestamps are not rewritten. There is no unbounded automatic archive expansion
for a single far-backdated arrival. An already fully loaded history includes
newly inserted older records on refresh.

Search is explicitly labeled as searching loaded messages. Whole-database
search and fully virtualized rendering remain separate future work.

Tests added (pending the coordinator's unified build at document creation):
- More than 1000 records with timestamp ties: no duplicates or omissions.
- New insertion between pages; cursor survival across Node restart.
- Illegal cursor and SQL-like ID input; conversation isolation.
- Local file transfer mapping remains in history pages.
- UI older-page anchor preservation and loaded-window retention on new arrival.
- Backdated live arrival does not automatically render an entire 850-row archive;
  explicit loading eventually reveals it without losing other records.
