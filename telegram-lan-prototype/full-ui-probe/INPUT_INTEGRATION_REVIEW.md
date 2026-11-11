# Full input / single-window review

Review scope: original InputField host lifecycle, focus, keyboard submit,
notifications and screenshot workflow. Source reviewed while single-window work
was in progress; function names are more stable than line numbers.

## Concrete findings

1. **Cross-conversation preedit state (fixed in adapter in this pass).**
   The host flag previously changed only on InputMethod. `setPlainText`/`clear`
   could replace the visible draft while keeping `preedit=true`, locking Enter.
   Native candidate state could also outlive conversation selection. These methods
   now cancel native IME only when the editor owns focus/composition, explicitly
   clear the original QTextEdit's preedit, then replace the draft. FocusOut also
   cancels preedit. Cancellation deliberately preserves committed text only.
   Native Windows IME behavior still needs a real-system test.

2. **Minimized notification activation (reported to coordinator).**
   `MainWindow::openConversation` full path used show(), not showNormal(), then
   unconditionally marked current conversation read. A minimized window can
   remain minimized while unread is cleared. Require restore + check visible /
   active before read acknowledgement. Test notification activation while
   minimized, not only synthetic WindowActivate on an ordinary shown window.

3. **Screenshot recipient is mutable across nested loops (reported).**
   `captureScreenshot` spans a 250 ms timer, capture overlay exec and confirmation
   dialog exec. `sendImage` finally reads m_current, rather than a frozen recipient.
   Programmatic notification activation can switch the conversation during these
   loops. Freeze recipient ID/display name at initiation or reconfirm if changed;
   never send to a new recipient merely because selection changed.

4. **Screen lifetime during capture (risk, not reproduced).**
   The delayed lambda captures raw QScreen*. External-display removal in the
   delay can invalidate it. Prefer QPointer<QScreen> and abort cleanly on removal.
   Test screenRemoved while capture delay is pending, or inject a screen provider.

5. **Screenshot cancellation focus (risk, not reproduced on Windows).**
   Restore path called show() without explicitly returning focus to the editor.
   In a single-window layout, Escape-cancel from overlay/preview should restore
   original window state and editor focus, preserve draft, send nothing, and
   retain pre-existing maximized state. Verify this independently of screenshot
   PNG generation tests, which do not exercise capture cancellation.

## Lifecycle review

- Existing qApp-null-safe teardown fixes the reproduced exit crash. Keep app exit
  tests after changes; don't regress by unconditionally dereferencing qApp.
- Per-editor style outlives InputField base destructor via QObject::destroyed.
  Changes that move deletion earlier can invalidate the base's const reference.
- Event forwarding uses a reentrancy flag and QPointer around synchronous events.
  Keep this protection when submitted handlers can synchronously close widgets.
- Host uses Qt event filter / singleShot rather than full Telegram Sandbox nesting.
  Reentrant modal dialogs, pending animations, immediate app shutdown and multiple
  MainWindow destruction/recreation deserve stress testing. Current tests establish
  normal paths, not a complete proof of arbitrary modal-loop safety.

## Additional tests beyond the prior suite

Added application/tests/test_telegram_input_lifecycle.cpp:
- A preedit -> replace draft B -> Ctrl+Enter submits B once, without old candidate.
- Clear during preedit -> insert new text -> Enter works.
- FocusOut cancels old candidate -> FocusIn/new IME commit -> one send.

These are synthetic Qt IME tests. They cannot establish native Windows candidate
window behavior or system notifications. Runtime result is recorded separately
in test-lifecycle.log. The earlier review-only candidate file is not a build target.
