# File result receipts (verified source-lab implementation)

A socket finishing its writes, an offer ACK, and a legacy file_release are not
proof of receiver SHA256 validation and final publication. Only an explicitly
versioned result from the expected receiving endpoint may update remote outcome.
This remains a trusted-LAN consistency check, not cryptographic authentication.

Required invariants:
- Receiver completed is emitted only after SHA256 validation and publishing the
  final file; rejection is an explicit receiver action. Pause/failure can be
  retried and must not be represented as permanent successful delivery.
- Sender binds transfer ID, declared receiver ID, captured endpoint, size, hash,
  version and monotonic result sequence. A peer record updated by the same incoming
  packet is not an independent identity proof.
- Persist sender receipt state before acknowledgement. A failed commit cannot
  return a success acknowledgement or leave a falsely durable success state.
- Receiver keeps unacknowledged results and retries across restart. Duplicate
  results are idempotent; conflicting same-sequence data is rejected; old results
  cannot regress a final outcome. Completion/rejection terminals are immutable.
- Legacy peers/releases remain explicitly unconfirmed, never guessed complete.
- Remote completion is separate from local received-file availability. A sender
  must not gain a received-file Open action solely because its peer completed.
- UI cards and transfer list display the same authoritative state and preserve
  last byte progress for failures/rejection. Do not retain a contradictory waiting
  invitation hint after an explicit final result.

Verified scope: real multi-node completion/rejection, pause/resume and
failure recovery; write-failure and lost ACK; sender/receiver restart; duplicate,
conflicting, wrong-peer/hash/size/version and late results; old protocol; callback
lifetime; full history/long-message/UI regression. Final FULL 17/17 (43.95s), default 17/17 (38.19s), three UI scale runs
22 passes each. ASAN test_files 29 passed in 4.178s, with prebuilt Qt
uninstrumented and leak detection disabled. Logs are in verification-latest
and verification-asan. An omitted pause progress guard was found in review and
fixed before these final full/ASAN runs.

## Explicit remaining limits

- These are unauthenticated LAN declarations. IDs and hashes are consistency
  checks, not signatures or verified human identity.
- The original receiving host address is bound to an outgoing invitation. An IP
  change can prevent acknowledgement of an old result; do not silently accept a
  new host just because that packet also updates peer discovery.
- Shared-directory downloads do not yet create an equivalent sender receipt
  record, so their offers explicitly omit result capability and use legacy
  release. They must not accumulate unacknowledgeable new result messages.
- Local final metadata and pending receipt share one atomic JSON snapshot. The
  published file and JSON are separate filesystem objects, however: a process
  crash between file publication and saving the snapshot can leave a file without
  a recorded final result. This implementation does not claim a cross-file
  transaction or general crash recovery for every filesystem boundary.
- Tests instrument application code when ASAN is run, not prebuilt Qt. Passing
  these selected paths is not a claim that every existing synchronous signal
  path in FileTransfer has been exhaustively verified.
