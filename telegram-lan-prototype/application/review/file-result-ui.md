# File result receipts: UI boundary

The sender card consumes only FileTransfer's persisted result state:
remote_completed = 对方已接收并校验, remote_rejected = 对方已拒绝,
remote_failed = 对方接收失败, remote_paused = 对方已暂停.
A local sent state still reads 已发送，最终结果未确认. Byte count reaching total,
a legacy release, or an absent result field never paints a verified result.

A remote completed result is not a local received file. Every sender state hides
and disables Open; the card signal handler and MainWindow recheck direction.
Deleting the original sender source file does not invalidate the fact that the
remote device verified its own copy. Incoming local completed retains actual
file/symlink/size checks and explicit user confirmation before opening.

Failed/paused progress displays the model's retained byte position. If the model
allows a sender retry, the button says 重新发送 and explains that it creates a
new invitation, rather than pretending to resume the recipient's paused task.
Final completed/rejected sender cards cannot pause or retry.

The transfer table derives status from the latest snapshot even when a delayed
legacy progress signal arrives, so it cannot overwrite a terminal result as
发送中. Status-bar invite hints are associated with one transfer ID and updated
only while the matching hint is still shown. A newer offer or an unrelated
storage/history warning is not overwritten; rejecting an offer no longer leaves
an obsolete instruction to receive it.

Timeline tests use real three-node transfer outcomes and wait for both sender
receipts before capturing group screenshots. UI-only edge cases cover retained
failure bytes, unknown legacy outcomes and absence of sender Open actions.
Only the coordinator's FULL-run screenshots should be published as FULL UI.

These are protocol receipts associated with the existing self-declared LAN peer
identity and transfer record, not cryptographically authenticated identity proof.
The UI does not claim that the recipient read the file or that all group members
completed; each target retains its own result.
