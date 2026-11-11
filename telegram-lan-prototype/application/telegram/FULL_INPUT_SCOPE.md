# Full original InputField integration scope

Enabled only with LANLINK_FULL_TELEGRAM_UI. Default remains extracted adapter.

- `telegram_full_input_adapter.h/.cpp`: newly written application glue around the
  complete original `Ui::InputField` class. Not a replacement implementation.
  Host event bridge follows upstream `Telegram/SourceFiles/core/sandbox.cpp`
  `crl::on_main_update_requests`/UpdateRequest contract. Leave subscriptions are
  no-ops on Linux as upstream's workaround applies only on macOS.
- `telegram_input_adapter.h`: build switch selects original-class adapter or old
  QPlainTextEdit adapter.
- `mainwindow.*`: uses shared adapter API for send/draft/font/emoji paths.
- full-ui-probe CMake imports complete original lib_ui and supporting libraries,
  codegen/styles/palette/emoji; no stubs or edited upstream InputField source.
- Original emoji resources in Telegram/Resources/qrc/emoji_*.qrc are linked to
  executables; official Qt imageformats WebP plugin is required.

Pins: Telegram Desktop c4bd064bcc84a359179e191fa6dfd216acda504f;
lib_ui 42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4.
Source URLs: https://github.com/telegramdesktop/tdesktop and
https://github.com/desktop-app/lib_ui. Submodule pins and licenses are preserved
in the source package. See ../LICENSE, ../LEGAL and DESKTOP_APP_LEGAL.

Tests: actual MainWindow editor, IME commit gate, Enter/CtrlEnter, exact 128 KiB
UTF-8 TCP delivery, over-limit draft retention, per-editor font changes and
emoji content. All 7 suites pass on Linux Qt 6.11.2. Windows/native IME behavior
and two physical machines remain unverified.

Application host currently uses Qt event filtering rather than the complete
Telegram Sandbox event-nesting machinery. Nested modal-loop lifecycle deserves
further production testing. No MTProto/cloud account service is implemented.
