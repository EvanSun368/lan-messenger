# File transfer / shared download review and regression results

Scope: `application/src/filetransfer.{h,cpp}`, existing file regression suite.
Only temporary directories and loopback peers were used. MainWindow shared UI
request-context filtering was assigned separately to the coordinator.

## Fixed concrete defects

1. **Offer notification before network acknowledgement.** `handle(offer)` called
   `registerOffer`, which synchronously emitted `offerReceived`. UI subscribers
   open modal confirmation/file dialogs. During that nested loop the remote
   request could time out and the accepted QTcpSocket could be deleted; after
   notification returned, `reply(s,...)` would access a stale socket. The service
   now stores/ACKs first, then queues UI notification. Added a regression where
   offer notification enters a nested event loop and the sender must receive ACK
   during that interval. This is a credible cross-platform crash risk, NOT proof
   that it caused the user's reported Windows shared-receive crash.

2. **Pause/reject ignored during SHA256 verification.** Late hash completion
   could still publish and report a file after cancellation. Pause now marks
   verification cancelled; reject also aborts active receiving. Late callbacks
   retain the part without publishing and cannot resurrect rejected offers.
   Verified pause/resume and reject/no-publication over two real loopback Nodes.

3. **Hash completion callback could synchronously destroy the service.**
   `runNextHash` previously accessed this after invoking an arbitrary callback.
   A QPointer check now prevents the use-after-free. Regression deletes the
   FileTransfer instance inside the callback.

4. **Duplicate fetch replaced active socket bookkeeping.** A second connection
   for the same ticket could replace m_active; disconnect of the old connection
   could remove newer state. Duplicate active fetch is now rejected, and cleanup
   removes only its own socket. Tested with a 16 MiB stream and throttled reader.

5. **Sending pump continued after a synchronous pause/abort in progress handler.**
   It now checks socket state and object lifetime after progress emissions and
   verifies write return values, rather than continuing to read and report data
   after cancellation.

6. **Shared directory context was absent from response signals.** Added
   `shareListedFor(peer,token,path,entries)` with captured request parameters,
   retaining old shareListed for compatibility. UI can reject stale responses.

## Evidence

`test-file-review.log`: **18 passed, 0 failed, exit 0**, Qt 6.8.3 Linux.
Includes previous transfer/resume/no-overwrite/Python interoperability tests plus:
- Actual shared nested Unicode filename download and zero-byte download.
- Correct peer/token/path response context.
- Pause during verify, then resume with matching hash.
- Reject during verify, no final file or completed signal.
- Callback destruction, concurrent ticket rejection, modal ACK ordering.
- Directory traversal inputs and symlink escape rejection.

## Remaining limitations

- No native Windows process or original reported crashing package was available.
  These Linux results do NOT establish that Windows receive crashes are resolved.
- Windows publication uses CreateHardLinkW. FAT/exFAT or other filesystems without
  hard-link support fail with a retained-part error; no compatibility fallback
  was introduced. This is distinct from a process crash.
- Filesystem path checks do not constitute protection against a hostile local
  process racing symlink/file replacement between checks and use.
- Protocol is for trusted LAN peers, not authenticated/encrypted hostile networks.
- Actual cross-machine Windows receive, cancel/resume, NTFS and exFAT destinations,
  antivirus/file locking and native UI modal shutdown remain necessary tests.
