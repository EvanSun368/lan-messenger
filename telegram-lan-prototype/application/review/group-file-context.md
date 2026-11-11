# Group file routing and trust boundary

New file offers carry optional group_context version 1 with complete id, name,
and fixed members. No context means legacy one-to-one routing. A supplied but
unsupported/invalid context is rejected, never silently downgraded to private.

Node::normalizedGroupDeclaration is read-only and shared by file and ordinary
chat entry points. It validates ID/name/member types and lengths, unique members,
and inclusion of both the sender ID and this receiver ID. Known groups require
the exact existing member set and keep the local canonical name. Incoming offers
or later group messages cannot rename or replace that group. File registration
commits a new group's metadata before returning a group conversation; failure
returns no route. Ordinary chat retains its original transaction combining the
new group and message, so a failed message cannot leave a newly created group.

The protocol has NO authenticated device identity, signatures or proof of member
consent. First-seen complete context is a trust-on-first-use LAN declaration,
consistent with existing group chat, not authorization proof. A malicious peer
able to claim another ID cannot be excluded by these structural checks. Use only
on a trusted LAN. UI group title tooltip and incoming group-file status explicitly
state the lack of identity verification.

Membership is NOT discovery. Context does not import arbitrary member hosts or
pretend other members are online. The sender's normal hello is still used for its
existing connection. A reply to an undiscovered member remains queued; only a
matching real ACK is reported received. The three-node test disables discovery
and uses a star topology to assert that distinction.

MainWindow routes unread counts, status and history from FileTransfer's validated
snapshot conversation_id, never directly from payload group IDs. Incoming group
file history uses the actual sender as author. New receiver group models appear
without needing a preceding group chat message. Legacy offers still appear in
the sender's direct conversation and require explicit user acceptance.

Tests added: invalid/version/membership/duplicate inputs, group persistence fault,
canonical name protection across file then chat and restart, new-group rollback
on chat save failure, and three real nodes receiving their first group file into
correct group cards/unread counters. Bob accepts, Carol rejects, with independent
real outcomes. Bob's later reply confirms Alice received but Carol stays queued
because Bob has not discovered Carol. Verification results are recorded by the
coordinator after the source is frozen.

Adjacent lifecycle audit: file group commit, received group/message notifications,
and peer notifications are queued with the Node as context. Received group then
message dispatch checks QPointer between signals, and handle protects sender
registration with Node/socket guards. Tests delete the receiving Node from real
peer/group signal handlers and verify the already-persisted reply/history remains
valid. This is a targeted fix, not a claim that every Node signal path is hardened.
Local createGroup also rejects control characters rejected by the shared wire
validator, so it cannot create an otherwise valid group that peers refuse solely
because local validation was weaker.

Lifecycle test refinement: destroying the Node also destroys its child sockets,
so an ACK queued for writing may be lost. The test does not require a dead
endpoint to guarantee network delivery. A sender must report either a matching
real ACK or explicit transport failure with its outbox still queued and no
received proof. After receiver restart, retrying the same message ID must obtain
ACK and keep receiver history at exactly one record. Hello similarly permits an
explicit connection error during destruction, followed by a valid restart reply.
No arbitrary delay or fake ACK is used to satisfy the tests.
