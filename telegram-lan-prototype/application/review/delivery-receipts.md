# Persistent delivery result ledger

Additive SQLite tables, no modification of historical message contents and no
wire protocol change:
- delivery_batches: message_id, recipient_count, created_at.
- delivery_targets: message_id+recipient primary key, name_snapshot, state,
  attempts, last_error, ack_at.

New outgoing messages freeze the recipient set in the same transaction as
outbox/history creation. A matching ACK writes received+ack_at and deletes that
recipient's outbox row in one transaction. Any write/commit failure rolls back,
retains the retry row, emits delivery_status_error, and does NOT emit delivered.
Permanent rejection records rejected with reason and retains the non-retrying
outbox row. Transport failure leaves queued with attempts/error. Successful
persistence emits delivery_changed; existing delivered event remains compatible.
There is no message cancellation API in current Node, so no cancellation path
was added or fabricated.

Node::deliveryStatus(messageId) returns known,total,received,queued,rejected and
targets containing recipient,name,state,attempts,error,ack_at. ack_at is null
without a matched persisted ACK. received means the peer acknowledged persistence,
NOT that a human read it. UI wording must never be 已读. A disconnected peer can
still show queued even though its process accepted the bytes but the ACK was lost;
retry deduplication resolves this without inventing confirmation.

Old messages lack batch records and return known=false, including old remaining
outbox rows after a new successful retry. Never reconstruct the original group
membership from a partial old outbox or from current group membership. Missing or
inconsistent new target sets also return unknown rather than imply success.

Tests cover partial group ACK/offline and restart, one group recipient ACK plus
one protocol rejection, SQL-trigger-injected ACK bookkeeping failure followed by
retry, historical messages with no outbox, and legacy pending rows after ACK.

Manual retry API: retryMessage(messageId) only accepts an existing pending
outbox ID with no in-flight callbacks. One transaction clears errors and restores
queued only for those pending recipients. Recipient IDs/count, message ID,
history, cumulative attempts and already-received target proofs are preserved.
On commit it emits delivery_changed and schedules flush; no new message is made.
Completed IDs, invalid IDs, in-flight IDs and persistence errors return false.
Legacy unknown status remains unknown. Tests inject a retry-update SQL failure to
verify both terminal state and outbox error roll back, then retry one rejected
group recipient to a real Node and verify one history record at each endpoint.
