# Input component source reuse

Upstream pinned `desktop-app/lib_ui` commit:
`42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4`.
Files: `ui/widgets/fields/input_field.h`, `input_field.cpp`.
Source URL:
https://github.com/desktop-app/lib_ui/blob/42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4/ui/widgets/fields/input_field.cpp

Actual adapted source: `InstantReplaces::add`, `TextOnly`,
`InputField::ShouldSubmit`, `processInstantReplaces` reverse-trie traversal.
The `heightAutoupdated` clamp approach is adapted (Qt plain document visual-line height is converted to pixels) without lib_ui styles/event helpers.
`TelegramInputAdapter` itself is new QPlainTextEdit integration glue, **not** the original
complete `Ui::InputField`. Rich markdown tags, custom emoji, quote formatting,
spellcheck, rpl streams and lib_ui animation/style infrastructure are not included.

Build tested with Qt6.8.3 GCC14.2 on Linux. Four functional tests passed:
submit/newline settings, typed replacements, IME composition, bounded auto-height
(Qt output is six including init/cleanup).

## Integration

Copy the four `telegram_input_*` source/header files into `application/telegram`.
Add `telegram_input_logic.cpp`, `telegram_input_adapter.cpp`, and header to the
Qt AUTOMOC target; include that directory. Replace QPlainTextEdit construction with
`TelegramInputAdapter`. Connect `submitted()` to normal validated send action;
map Enter/CtrlEnter settings with `setSubmitSettings`. Disable old key event
filters that intercept Enter first. Keep send-button validation and long-message
limits in the existing transport/UI path. This widget does not send itself.

Automatic punctuation replacements are disabled by default so technical code
and logs are not silently modified. Enable explicitly if desired. Pasted text is
plain text and is not passed through automatic replacement. IME Return is not
intercepted while preedit is active.

The auto-height is disabled by default and is enabled via `setHeightRange`; for an existing manually resized
composer, set a range compatible with its splitter or adapt this wrapper.
