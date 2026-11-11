# Original-control group and contact-profile layers

`telegram_contact_boxes.h` is new LAN integration code using unchanged original
Telegram `Ui::GenericBox`, `LayerManager`, `InputField`, `Checkbox`, `FlatLabel`
and `LinkButton`. It is not a copied Telegram cloud group/member controller.
Original sources are pinned in the existing
source ledgers (Telegram c4bd064bcc84a359179e191fa6dfd216acda504f,
lib_ui 42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4). Their upstream GPL license
and copyright notices remain applicable; the combined laboratory follows GPL-3.0-or-later and retained upstream terms.

Group names and checked member IDs are passed to the existing Node createGroup
operation. Empty selection does not mutate state; failed creation keeps the
layer open. Contact profiles are shown only after the existing profile_received
event, bound to its returned peer ID. They are unauthenticated LAN declarations,
not verified Telegram accounts. Narrow resizing uses actual original box width
and recentering methods. No implicit publishing or device access is performed.

Tests exercise real original controls, cancel/commit failure, frozen profile IDs
and narrow hosts. These original primitives are genuinely reused, while the
LAN-specific content/layout/controllers remain newly authored adapters.
