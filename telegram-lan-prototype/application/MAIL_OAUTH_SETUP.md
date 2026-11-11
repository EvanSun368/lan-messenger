# Gmail / Outlook OAuth setup and support boundary

Implemented: authorization code with S256 PKCE in the system browser; random,
one-use state; local IPv4 loopback callback; token exchange; refresh before explicit mail operations or enabled background polling; IMAP and SMTP XOAUTH2. Provider token URLs and OAuth mail
servers are fixed in code. There is no password fallback from OAuth.

This is implementation plus local fake-server validation, **not live Gmail or
Microsoft acceptance**. No real accounts, real credentials, or external email
were used in development tests. Registration, tenant policies, consent review,
conditional access, and provider interoperability still need owner-authorized
acceptance. Opening an unconfigured mail window never connects. Saving usable credentials or
finishing authorization can start polling only for an account whose automatic sync
option was enabled. Neither saving, authorization nor polling sends mail.

## Credential storage and platform requirement

The production backend currently uses Windows Credential Manager. Linux/macOS
have no integrated credential backend and explicitly disable authentication; no
plaintext fallback is offered. Windows-native build/runtime acceptance is still
pending. Large credential bundles exceeding Windows Credential Manager limits
fail explicitly. Do not interpret a Linux mock test as real system-store proof.

Only public client ID and Microsoft tenant are saved in accounts.json. Refresh
tokens and an optional Google desktop-client secret are saved as an account-bound
bundle in the system credential store. Access tokens are used transiently and
never included in that bundle. No client registration or secret is embedded in
this project. Qt/network buffers can retain transient copies in process memory;
this implementation does not claim hardware-backed memory or secure zeroization.

Deleting a local account deletes its local stored credentials; it does **not**
revoke the grant at Google/Microsoft. Revoke that grant in the provider's account
security settings when appropriate.

## Google Gmail

1. In your own Google Cloud project, configure the OAuth consent screen and create
   an OAuth client of type **Desktop app**. Do not use a web/confidential client.
2. Allow the mail scope https://mail.google.com/ in your consent configuration.
   This broad restricted scope is required for Gmail IMAP/SMTP, not just message
   sending. Production distribution can require Google's verification/security
   assessment. Testing-mode apps can have restricted test users and short-lived
   refresh grants; follow Google's current policy.
3. Add the Gmail template in LanLink and enter the desktop client ID. If the
   desktop registration supplies a client secret needed by its token endpoint,
   enter it in the masked Google client-secret field; it goes only into the system
   store. A native client secret cannot make a distributed desktop app confidential.
4. Save, then explicitly click **浏览器授权 / 重新授权**. The app uses
   http://127.0.0.1:<ephemeral-port>/oauth/callback for the desktop loopback flow.
   Check the account and requested scope in the system browser.
5. Return to LanLink. Accounts with automatic sync enabled can start polling after authorization.
   Otherwise choose Refresh explicitly. Sending always requires its own action and
   confirmation. Gmail account/Workspace policies must allow IMAP.

A separate Gmail app-password template remains available only when the account's
2FA and organization policies allow an application-specific password. Do not put
an ordinary Google login password in that template or an OAuth token in it.

## Microsoft Outlook.com / Hotmail / Microsoft 365

1. Register your own **public native/desktop client** in Microsoft Entra. Choose
   supported account types to match the accounts you intend to authorize.
2. Configure the mobile/desktop redirect URI
   **http://localhost/oauth/callback**. The app chooses a fresh local port at run
   time, using Microsoft's localhost loopback port matching. Do not configure a
   web application redirect that requires a confidential client secret. Follow
   the current Entra registration UI for public-client desktop authorization.
3. Grant delegated Exchange Online permissions for IMAP.AccessAsUser.All and
   SMTP.Send, and allow offline_access. The requested resource scopes are
   https://outlook.office.com/IMAP.AccessAsUser.All and
   https://outlook.office.com/SMTP.Send. The tenant may require admin consent.
4. Choose **Outlook.com / Hotmail（个人邮箱）** for personal Microsoft accounts,
   with the default consumers tenant, or **Microsoft 365（企业 / 学校）** for
   work/school mailboxes. Enter the registration's application/client ID. Set
   tenant to common, organizations, consumers, a tenant ID, or its verified domain
   according to the registration. Do not enter an OAuth URL or client secret.
5. Ensure the mailbox has IMAP enabled and the tenant/mailbox permits SMTP AUTH.
   OAuth support does not override disabled SMTP AUTH or organization policies.
6. Save and click **浏览器授权 / 重新授权**. Check the account and permissions.
   After authorization succeeds, enabled automatic sync may fetch the inbox;
   otherwise choose Refresh. Sending still requires an explicit action and confirmation.

Outlook OAuth tokens are never routed to custom hosts: the template pins
outlook.office365.com:993 with implicit TLS. Personal Outlook.com/Hotmail uses
smtp-mail.outlook.com:587 with STARTTLS; Microsoft 365 uses smtp.office365.com:587
with STARTTLS. Gmail similarly pins its own official hosts and ports. For an unrelated
enterprise server, use the manual password template; custom OAuth providers are
not implemented.

## Failure behavior

- Cancel or a three-minute browser timeout closes the callback listener. Token
  requests time out after at most 30 seconds. Repeated actions are disabled while
  an operation is pending; canceled responses cannot trigger mail submission.
- Callback method, path, Host, duplicate parameters, and state are checked.
  Authorization codes are exchanged once. Bad unrelated callbacks cannot provide
  credentials or overwrite account settings.
- Token requests require normal certificate/hostname validation, TLS 1.2+, and
  do not follow HTTP redirects. Remote error bodies are not displayed or logged.
- Refresh-token rotation is persisted before mail authentication. If system-store
  saving fails, the app does not connect to the mail server. An expired/revoked
  grant produces an explicit reauthorization instruction; mail is never silently
  retried or resent.
- Account credentials are bound to account identity, provider, client ID, tenant,
  and mail hosts. Changing identity/registration requires adding a new account.
- The plaintext loopback token endpoint override is a separate code-only fixture
  constructor for tests. It accepts only numeric loopback HTTP addresses and is
  not exposed in account settings. Production endpoints remain pinned HTTPS.

## Sources for registration and provider policy

- Google native-app OAuth: https://developers.google.com/identity/protocols/oauth2/native-app
- Gmail XOAUTH2: https://developers.google.com/workspace/gmail/imap/xoauth2-protocol
- Outlook.com server settings: https://support.microsoft.com/en-us/outlook/pop-imap-and-smtp-settings-for-outlook-com
- Microsoft IMAP/SMTP OAuth: https://learn.microsoft.com/en-us/exchange/client-developer/legacy-protocols/how-to-authenticate-an-imap-pop-smtp-application-by-using-oauth
- Microsoft redirect URI restrictions: https://learn.microsoft.com/en-us/entra/identity-platform/reply-url

Provider policies and console labels can change; these links are registration
references, not evidence that a real account has passed this project's tests.
