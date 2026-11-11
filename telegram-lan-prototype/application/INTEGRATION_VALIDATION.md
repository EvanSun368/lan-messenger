# Integration verification: 2026-11-11

## Final serial matrix: passed

The final frozen-source matrix finished successfully on Linux with Qt 6.11.2,
GCC 14, CMake and the offscreen Qt platform. Both configurations built all targets
serially; tests used isolated HOME/XDG settings and local fake mail servers.

- FULL original-control build: 34/34 CTest groups passed, zero failures, 89.96 seconds.
- Default Qt build: 34/34 CTest groups passed, zero failures, 58.74 seconds.
- FULL at 125%: chat UI 26 cases passed (18.347s); mail UI 21 passed (4.528s).
- FULL at 150%: chat UI 26 cases passed (18.251s); mail UI 21 passed (4.947s).
- FULL at 200%: chat UI 26 cases passed (19.855s); mail UI 21 passed (5.035s).

QtTest case totals include setup/cleanup. The FULL aggregate has one internal
skip: real OS interface enumeration is unavailable in the sandbox (AF_NETLINK).
The default aggregate has 24 internal skips, including original-control-only cases
and the same unavailable interface enumeration. The 34 passing CTest groups do
not mean those skipped paths were exercised. All three FULL chat/mail DPI suites
have zero internal skips.

Full-mode focused totals in this same aggregate include OAuth 51, MIME 14, mail
transport 19, sync state 7 and mail-window 21 cases. The real interface/VPN, native
notification and provider limitations below remain open.

Logs and the PASSED marker: `../full-ui-probe/verification-github/`. They include
build output, each aggregate CTest summary, detailed per-case output and all six
DPI logs. Earlier focused baseline logs are retained under `review/` and are not
substitutes for this final matrix. Source-scope/licensing files describe which
controls are original upstream code and which controllers/layouts are custom.

The earlier interrupted build left two zero-byte object files. Those generated
objects were rebuilt from the genuine upstream sources, with no source stub or
disabled test. Original mail-button host padding was fixed and the final source
was exercised again in the matrix above.

Publication is a separate GitHub branch, not a merge or executable release. The
first source commit is `d2bbb56f97699c1e66a1ef229e1e404f87e6bb62`; this record is
added/updated by a follow-up verification commit. No GitHub Actions run or status
check existed for that first source commit, so local passes are not called CI
passes. No pull request has been created.

## Source and credential review

- Telegram Desktop pin: `c4bd064bcc84a359179e191fa6dfd216acda504f`.
- lib_ui pin: `42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4`.
- All 33 entries in `../emoji-component/SOURCE_MANIFEST.json` matched their stored
  SHA-256 values. The three original suggestion-engine files and the application
  GPL/LEGAL texts also matched the local pinned upstream copies byte-for-byte.
- GPL-3.0-or-later terms, Desktop App legal notices, source ledgers, original
  emoji generator/inputs and pinned upstream acquisition script are included.
- The new publication files were screened for private-key blocks and high-confidence
  GitHub/AWS/Stripe/Google API-key patterns; no matches were found. This is not a
  claim of a comprehensive third-party security audit.
- Runtime account data, credentials, disposable TLS private keys, downloaded SDKs,
  build outputs and source archives are excluded. Tests use explicit fixture values
  and local fake servers, not real accounts or external email transmission.

## Unverified acceptance and product limits

- Windows-native builds, application runtime and Windows Credential Manager behavior
  have not been accepted on a Windows device. The earlier reported Windows receive
  crash is not claimed fixed.
- FULL UI currently has a Linux-only build host; production credential storage is
  Windows-only. Linux/macOS mail authentication fails closed. There is no tested
  end-to-end platform combining FULL UI and a production credential backend yet.
- Real Gmail/Outlook/other providers, app-registration/tenant policies, OS tray
  delivery/click routing, real VPN routing and two physical/cross-city machines
  remain unverified.
- The VPN policy does not create/verify a VPN, force OS routing, authenticate the
  remote application identity, or add application E2EE. No VPN, firewall, route or
  security setting was installed or changed.
- New mail is polled, not IMAP IDLE. There is no complete mailbox/folder/flag mirror,
  server Drafts/Sent APPEND, production release, or full Telegram application parity.
- Existing repository Windows workflows build the historical `cpp/` tree, not this
  new integration directory. Their results cannot validate the new FULL UI.
