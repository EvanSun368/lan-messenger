# Original settings-box integration

Pinned upstream: Telegram Desktop c4bd064bcc84a359179e191fa6dfd216acda504f,
lib_ui 42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4. Original GPL/LEGAL retained.

telegram_profile_box.h is a new LAN host adapter, not copied Telegram account
settings. It instantiates unmodified Ui::GenericBox, LayerManager, BoxLayerWidget,
InputField, FlatLabel, SettingsButton and Checkbox from the upstream lib_ui tree.
Box and field styles, keyboard editing and layer painting are actual upstream
implementations. The host provides five public-profile fields, privacy warning,
font callback and notification callback; Node retains validation/persistence.

Saving fails without closing if Node refuses the profile. Cancel does not save
profile edits. Notification changes preserve the prior immediate-effect behavior.
Duplicate invocation reuses an open overlay. Parent destruction, close and focus
restoration use Qt/rpl lifetimes and QPointer, not a nested QDialog event loop.

At runtime resize the host clamps width to min(440,parentWidth-32), lets original
content wrappers reflow, then calls original BoxLayerWidget::parentResized. The
last call is essential: original setDimensions updated width/height but retained
the x position calculated for the prior 440px box, yielding x=-20 at400px host.
The original recentering path produces x=16 with368px width. No manual placement
of the individual fields or action buttons is introduced.

Tests cover rejected/accepted save, cancel, duplicate opening, callbacks, parent
destruction and a single overlay resized800→400→800. Entire layer bounds, actual
button hit testing and draft preservation are asserted. Original font selection
still opens the existing Qt font dialog; other auxiliary LAN dialogs are not
claimed as migrated by this component. See VALIDATION.md for final run evidence.
