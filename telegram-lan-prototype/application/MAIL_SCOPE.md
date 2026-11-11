# Independent mail module (implementation stage)

The mail module is separate from LAN discovery, Node identities, shared-file
permissions, UDP/chat delivery and Telegram cloud accounts. Its initial protocol
surface includes IMAP inbox polling with UIDVALIDITY-aware notification deduplication,
bounded header summaries, read-only full-message fetch, editable threaded replies
and explicit SMTP submission. It does not implement a complete mailbox mirror.
See MAIL_SYNC_UI.md for synchronization, UI and lifecycle details.

## Security and honest support boundary

- Production endpoints require implicit TLS or STARTTLS with normal certificate
  and hostname verification, TLS 1.2 or later. No certificate-ignore switch and
  no fallback to unencrypted password submission are offered in account settings.
  Enterprise private CA certificates must be properly trusted by the system.
- Windows Credential Manager storage is implemented, but Windows-native build
  and runtime acceptance are still pending. Other platforms fail closed when no
  supported system credential backend is available; no plaintext fallback exists.
- Account metadata contains no password or token. The native credential store is
  separate from JSON and LAN identity. Only accounts with explicitly enabled sync
  and usable saved credentials are polled at startup. Opening an unconfigured
  window never initiates authorization or sends email.
- QQ and 163 are connection templates, not evidence of live provider acceptance.
  Manual configuration allows internal/external hostnames and IPv6 with TLS.
  Gmail and Outlook OAuth templates implement public-client authorization code
  with S256 PKCE, loopback callback, token exchange/refresh and IMAP/SMTP XOAUTH2.
  They require the user's own app registration and provider policy permissions;
  no live-provider acceptance is claimed. See MAIL_OAUTH_SETUP.md. A separate
  Gmail app-password template is available only for accounts whose policies allow
  application-specific passwords after 2FA; ordinary account passwords are not
  supported. No live Gmail acceptance has been verified. Outlook also requires
  enabling IMAP in the service settings.
- SMTP final DATA 250 means server acceptance, not delivery to the recipient or
  read acknowledgement. Loss after submitting DATA without final confirmation is
  an unknown outcome; the UI must not suggest blindly resending.
- MIME rendering is plain text. HTML-only mail is converted with a resource-denying
  document; remote images, styles, local files and automatic links are not loaded.
  Attachments are data requiring an explicit save action, not automatic execution.
- No real account credentials or external emails are used in development tests.
  An explicitly named loopback-only plaintext transport policy exists solely for
  local fake-server tests and is not exposed by account settings or templates.

## Not yet included

Folders beyond the initial INBOX flow, IDLE/push, full/offline mailbox caching,
server-side search, drafts/sent-folder APPEND, flags/delete/move, complete MIME
legacy encodings/address grammar, provider-specific authentication variations,
calendar/contact integration and a Windows distributable are not claimed.
The email controller and account/settings/tab scaffolding are custom. FULL mode
reuses actual Telegram list presentation, input fields and action controls, not a
Telegram mail subsystem (Telegram itself is not an IMAP/SMTP email client).
The current FULL host is Linux-only while native credentials are implemented only
on Windows. Mock FULL testing is not a verified real-mail product configuration.

Final validation results are recorded in VALIDATION.md after integration.

## Local contact emails and explicit chat export

Contact email addresses belong to a separate local-only address book keyed by
LAN peer ID. They are not added to public profile replies or discovery packets.
The contact-profile editor explicitly states this scope. They do not imply a
verified identity; check the actual email recipient before submission.

The chat export dialog freezes the current conversation and currently loaded
history. Every row is unchecked by default; selecting all means only the visible
loaded range. The pure export helper validates each selected message ID and its
conversation binding, rejects ambiguity/duplicates/missing IDs and never reads
attachment paths or other chats. Images/files become text placeholders, not
implicit attachments. The user may select a local contact address or edit it.
Export creates a previewable draft only. Existing draft/attachment replacement
requires explicit confirmation, and final SMTP sending has its own confirmation.
