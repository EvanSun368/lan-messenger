# File-transfer auxiliary UI boundary

`TransferList` and `ShowTransferBox` are LAN presenters, not Telegram's Session-backed document/history class. Actual upstream controls compiled and used: `Ui::RpWidget`, `Ui::ScrollArea`, `Ui::FlatLabel` (real text shaping), `Ui::LinkButton` (real click interaction), `Ui::GenericBox` and `Ui::LayerManager`. Layer hosting reuses the application's bounded original-contact-box host. No visible QDialog/QTableWidget is needed in the FULL integration.

File state comes only from `FileTransfer::transferSnapshots` / `transferChanged`; `upsert` performs no networking and does not accept any offer. The action callback routes to the host's existing consent/directory/reject/pause/retry/open logic. Status wording deliberately reuses `TransferCard::statusText`, including remote verified/legacy-unconfirmed distinctions. Incoming Open checks state, regular file, no symlink and expected size again at click. Outgoing remote completion never grants Open. Failed bytes remain shown. The thin progress bar and row geometry are a custom RpWidget painter, not upstream Telegram file-message artwork.

The original QTable implementation may remain only in the default build. The shared-file publish/download window is a separate workflow; token exposure, directory consent and frozen peer/token routing must remain in the host and are not migrated by this helper.

Tests cover real original control instances, waiting-state no implicit accept, accept callback, failure bytes, outgoing verified completion without local Open, file removal between snapshot and click, and actual original-layer close/lifetime. Test results must be reported only after execution.
