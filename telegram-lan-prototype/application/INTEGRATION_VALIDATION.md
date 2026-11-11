# Integration verification: 2026-11-11

## Current status

Source is frozen for the Telegram UI / mail OAuth-polling-reply / managed-VPN
integration. This is a review branch, not a production release or merge request
that has passed all platform acceptance. The final FULL/default aggregate and DPI
matrix is still running; the record will be updated when its results are known.

Verified before the first source commit:

- Default Qt 6.11.2: all targets built; 34/34 CTest groups passed, zero failures,
  58.36 seconds. See `review/integration-default-ctest.log`.
- FULL original Telegram controls: mail-window suite 21/21 QtTest cases passed,
  4.575 seconds, including setup/cleanup. See `review/integration-full-mailwindow.log`.
- Final FULL mail screenshots were inspected after fixing original-button host
  padding and disabled-state visibility. They use labelled mock accounts, not real
  mailboxes. See `screenshots/mail-window-{inbox,compose}-mock.png`.
- The earlier standalone OAuth suite passed 51 cases. This focused result does
  not substitute for the currently running aggregate matrix.

The FULL rebuild had two zero-byte object files after an interrupted earlier
process. Only those rebuildable objects were removed and regenerated. No upstream
source was stubbed or altered to bypass missing symbols. FULL-only button sizing
was then fixed and its mail suite rerun. The final build is serial to avoid the
previous memory pressure from overlapping compilations.

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
