# Single-window layout migration

Pinned upstream Telegram Desktop c4bd064bcc84a359179e191fa6dfd216acda504f,
GPL-3.0-or-later with upstream OpenSSL exception.

telegram_column_layout.h adapts:
- Telegram/SourceFiles/window/window_session_controller.cpp:
  SessionController::computeColumnLayout, countDialogsWidthFromRatio.
- Telegram/SourceFiles/window/window.style:
  columnMinimalWidthLeft=260, columnMinimalWidthMain=380.

Retained algorithm: below summed minima use one-column layout; otherwise round
bodyWidth*dialogsRatio, clamp to left minimum and bodyWidth-main minimum. Both
visible columns consume available width. The ratio comes from the local splitter.
Third columns, filters, session settings and Telegram navigation are excluded.

mainwindow.cpp is NEW Qt integration, not a copy of upstream Window/Controller.
LANLINK_FULL_TELEGRAM_UI=ON embeds existing LAN conversation and list in one
window. Narrow widths show either list or conversation with a back button.
Opening a conversation restores a minimized window. Capture hides the whole
window. Current-conversation notification suppression follows main-window
activation rather than the old detached chat. OFF retains detached windows.

Tests cover 639/640 width boundary, ratio clamp, real embedded-vs-window identity,
wide/narrow/back/wide transitions and minimized restore. No claim of full original
Telegram Window migration, and no Windows runtime validation.
