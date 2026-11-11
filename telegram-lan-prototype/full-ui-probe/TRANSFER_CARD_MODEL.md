# Real transfer card model and actions

`FileTransfer` now stores durable transfer records alongside pending tasks in its
existing state file. `transferSnapshot(id)` / `transferSnapshots()` report actual
service state; `transferChanged(snapshot)` is context-bound queued/coalesced by ID,
so card subscribers cannot delete the service in the middle of state mutation.

Fields: id, peer_id, conversation_id, direction, name, total, done, state, status,
path, save_dir, sha256, timestamp (seconds with millisecond precision), can_retry,
persistence_error. `offerFile(peer,path,conversationId={})` freezes local sender
conversation context without changing the wire protocol. Retry preserves it.

States are waiting, running, verifying, paused, failed, rejected, sent, completed,
validating, missing. Sender `sent` means only sent and awaiting remote verification,
not confirmed completion. Receiver `completed` is set only after SHA256 validation
and safe final-file publication. On restart completed records become validating
until actual local file hash/size is checked; changed/missing/symlink files are not
presented as verified. Failure and pause preserve the last real byte position.

QSaveFile open/write-length/commit errors are checked, logged and asynchronously
reported once per distinct failure; snapshots expose persistence_error and the
card marks state unsaved. A recoverable disk failure does not falsely deny that
file content was transferred, but warns that history may be lost on restart.

`TransferCard` is a new original Qt component, not claimed as upstream Telegram
file-card code. It displays filename, real bytes, progress, and concise status.
It emits accept/reject/pause/retry/open requests; it does not fabricate transfers
or independently open files. Open is available only for completed records with
an existing non-symlink ordinary file matching size, and rechecks on click.
The main integration independently rechecks and calls the appropriate service.

Validation (default Linux Qt build):
- test_transfercard: 4 total passed, action gating and removed-file open protection.
- test_files: real loopback Unicode/empty shared-file transfer, pause/resume,
  rejection with retained progress, restart hash validation and same-size content
  change detection, persistence failure/recovery, queued subscriber destruction,
  and prior security/lifecycle/interop cases. See test-files-card-model.log.
- Actual MainWindow timeline/group/scroll integration is tested separately by
  dialogs_component; this model report does not claim those results prematurely.

No Windows execution was performed. Native Windows receive-crash resolution,
NTFS/exFAT compatibility and shell open behavior remain unverified here.
