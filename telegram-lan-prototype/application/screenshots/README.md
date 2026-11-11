# Screenshot provenance

Current original-control-shell previews: `original-shell-wide.png`,
`original-shell-narrow.png`, `original-shell-menu.png`. These are actual FULL-mode
Qt window/widget grabs with demo conversation data. The menu is a standalone
original PopupMenu grab, not pasted onto a window image.

Current real file-test captures are copied from `full-ui-probe/verification-latest`
after verification (`telegram-file-timeline-*`, `telegram-group-file-*`). Those
exercise real local nodes and file outcomes; do not interpret demo messages as tests.

Other PNG files in this directory are historical stage artifacts. They must not
be presented as screenshots of the current shell or evidence of full Telegram UI
parity. No image here is a Windows-native screenshot.

Current original settings captures: `original-settings-box.png` is the actual
FULL application opening its real settings callback; `original-settings-narrow.png`
is the same original GenericBox helper in a 400px test host with demo profile data,
after a wide-to-narrow resize. Neither is a Windows-native screenshot.

## Current Message and auxiliary-layer stage

`original-group-box`, `original-contact-box`, `original-transfer-box`, and
`original-sharing-box` are actual FULL application's original-layer controls.
`original-create-group-narrow`, `original-contact-profile-narrow`, and
`original-sharing-box-narrow` are standalone narrow-host tests with synthetic data.
`original-transfer-live` and `original-transfer-live-sender` show real TCP
receiver/sender rejection after an actual original-button click.
`original-sharing-live` shows an actual LAN directory listing with edited draft
inputs; returned file actions deliberately retain the accepted request context.

`original-group-identity` is a deliberately injected row-presentation test in
the demo window, not a real group creation or network screenshot; surrounding
roster/demo data are not an end-to-end group UI example. Do not use it as proof
of real group delivery. Use real telegram-group-file-* captures for that.
All new captures are Linux offscreen Qt grabs, not Windows images.

## Email stage

mail-window.png is the real FULL application opening the independent Qt mail
window with no configured account. mail-chat-draft.png uses actual local test
Node history, selecting one message and pre-filling a local recipient address;
it is a draft only, not a sent email. Linux's unavailable credential backend
warning is intentional.

mail-account-configuration-mock.png, mail-window-inbox-mock.png and
mail-window-compose-mock.png use injected mock credentials/transport with fake
example.invalid accounts. They show real widgets but are not live provider
interoperability evidence, Windows images, or Telegram-original mail UI.
