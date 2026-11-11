# Versioned group-file offers, compatibility and limits

## Wire contract

A normal `offer` may carry `group_context`:

```json
{"version":1,"id":"32-hex-group-id","name":"Group name","members":["member-id", "member-id"]}
```

The sender obtains this only from its local Node group model and checks that the
individual target is a member. The receiver validates through Node's unified
`acceptFileGroupContext` policy: exact numeric version 1; valid unique member IDs;
local receiver and request sender included; bounded valid name; existing group
membership set must match, and its local name is not overwritten.

An unknown group is registered from the full declaration under the same TOFU model
as existing group messages. Persistence must succeed before acceptance. This permits
a group's first message to be a file, without inventing a trusted group from a bare
ID. Invalid supplied context is rejected, never silently downgraded.

**群成员信息来自局域网设备，未进行身份认证，仅适用于可信局域网。**
The transport has self-declared peer identities, not signatures/authentication.
Membership consistency checks are not proof of identity or member consent.

## Compatibility

No context: unchanged one-to-one offer semantics. Existing legacy receivers may
ignore unknown fields and display the file in a direct conversation.

New ACKs include `conversation_id` and `group_context_version`. A sender records
remote group route confirmed only for exact numeric 1 and the expected string
conversation ID. Missing/unsupported/mismatching ACK information remains
`remote_group_route=unconfirmed`; the card explicitly warns that older peers may
display the transfer in direct chat. The file invitation still proceeds.

Shared-directory downloads are NOT group-creation entry points. Their nested offer
must be type=offer, from the requested peer ID, and have no group_context. An invalid
response creates neither incoming task/group nor partial destination file.

## Replay and lifecycle

Previously recorded, pending incoming or local outgoing transfer IDs cannot be
reused as new offers. Legal resume uses fetch with the existing ticket; a new user
send/retry creates a new UUID. Rejection of a replay does not release or abort the
receiver's original task. A first-offer callback also does not abort an already
active same-ID sender transfer on a failed duplicate acknowledgement.

Node group notifications are queued. FileTransfer additionally guards owner/node
and socket lifetimes across registration and the handle/downloadShare callers.

## Verified

Default Linux `test_files`: **26 passed, 0 failed, exit 0**, recorded in
`test-group-protocol.log`. New coverage includes:
- Three real loopback Nodes: first group item is a file; both receivers route to
  the group and complete SHA256-checked downloads only after explicit acceptance.
- Member mismatch, duplicate member, non-member sender, missing receiver, bare
  group ID and unknown version rejected; known local name is not overwritten.
- Completed-ticket replay and collision with a local outgoing ticket rejected.
- Legacy no-context direct route and old ACK unconfirmed route.
- Group event destroys receiver Node and its file service without crashing.
- Shared-response wrong known identity, wrong type and unexpected group rejected.

This does not establish native Windows behavior or authenticated network security.
Actual multi-window timeline rendering is validated separately by its owning task.

## Receiver result protocol v1
Ordinary and group file offers now advertise `result_version: 1`; the offer ACK also advertises it. After SHA256 verification **and successful no-replace publication**, the receiver queues a durable `file_result` containing version=1, transfer id, sender identity declaration, original size/SHA256, increasing sequence, and state. States are completed/rejected (immutable terminal) or failed/paused (retryable). This declares peer-reported results, not cryptographic proof of remote storage.

Sender checks original persisted peer ID **and original offer peer_host**, id, size, hash, exact numeric version and integer sequence. Current discovered peer address alone is not used as self-verifying evidence. IP changes can prevent receipts until original address is restored. Equal-sequence equal-state replay ACKs idempotently; conflicting/older or post-terminal new results are rejected. Remote status is persisted before ACK; failed persistence rolls back visible success and does not ACK.

Pending results persist with receiver records, retry every five seconds while peer reachable, and survive service restart. ACK binds version/id/sequence; a superseded pending sequence is not cleared. Old `file_release` only frees a source ticket and never implies completion. Old peers retain “sent, not confirmed” semantics. Shared-download tickets currently have no sender timeline record, so their offers deliberately **do not advertise result_v1** and retain legacy release (no invented sender completion).

Sender states: remote_completed, remote_rejected, remote_failed, remote_paused; fields remote_result, remote_result_sequence, remote_result_at. Final states never open sender source files as received files. Receiver pause is resumable; rejection terminates the invitation, preserving any partial file without publishing. Failure keeps real byte progress. Sender retry creates a new invitation/id; receiver retry resumes same invitation when available.

Linux loopback tests exercise actual completion, hash corruption, pause/resume, rejection, receipt retry after both services restart, duplicate/conflicting/version/hash/size/peer/id validation, legacy release, and storage-write failure without false ACK. These are not Windows crash validation or authenticated-network tests.
