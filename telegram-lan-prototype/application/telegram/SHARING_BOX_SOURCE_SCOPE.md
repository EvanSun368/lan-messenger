# Original-control shared-file helper

`telegram_sharing_box` uses unchanged Telegram lib_ui GenericBox, LayerManager, ScrollArea, VerticalLayout, FlatLabel, LinkButton and InputField. It is a LAN-specific sharing presenter, not Telegram cloud saved-files/media UI. The small host/layout and callback wiring are new code; no upstream Session or cloud identity is emulated.

Publishing, revoking, listing and downloading are **not performed by the helper**. Main retains folder selection, publication consent, clipboard operation, existing FileTransfer methods and download-directory selection. Opening the box never publishes or downloads. Each displayed root action captures its token. Remote results are accepted only for the requested peer/token/directory. Each remote action captures the context of its accepted response; later input edits or conversation changes cannot retarget a download. Starting a new browse clears old actionable rows. Unknown directory/file flags preserve the existing protocol convention.

The standalone test invokes actual original buttons, checks no implicit publish, copy/revoke token identity, rejection of stale results, and captured download/enter context after editing the fields. Application network tests and native directory-choice behavior remain separate and must be verified by the integrator.
