#pragma once
// LAN host factories. The actual controls, paint, ripple, keyboard navigation
// and menu layout are unmodified Telegram lib_ui (GPL-3.0), not Qt lookalikes.
// Only available when LANLINK_FULL_TELEGRAM_UI is enabled.
#include "ui/ui_pch.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "styles/style_widgets.h"
#include "styles/style_lan_original_dialogs.h"

void ensureTelegramUiRuntime();

inline Ui::IconButton *telegramHamburgerButton(QWidget *parent) {
    ensureTelegramUiRuntime();
    return new Ui::IconButton(parent, st::lanDialogsMenuToggle);
}
inline Ui::IconButton *telegramMenuButton(QWidget *parent) {
    ensureTelegramUiRuntime();
    return new Ui::IconButton(parent, st::menuToggle);
}
inline Ui::IconButton *telegramBackButton(QWidget *parent) {
    ensureTelegramUiRuntime();
    return new Ui::IconButton(parent, st::backButton);
}
inline Ui::PopupMenu *telegramPopupMenu(QWidget *parent) {
    ensureTelegramUiRuntime();
    auto *menu = new Ui::PopupMenu(parent, st::defaultPopupMenu);
    menu->deleteOnHide(true);
    return menu;
}

inline Ui::IconButton *telegramSearchButton(QWidget *parent) {
    ensureTelegramUiRuntime();
    return new Ui::IconButton(parent, st::lanTopBarSearch);
}
inline Ui::IconButton *telegramAttachButton(QWidget *parent) {
    ensureTelegramUiRuntime();
    return new Ui::IconButton(parent, st::lanHistoryAttach);
}
// The original IconButton with upstream SendButton.inner visual style.
// This is NOT the full Telegram SendButton (recording/payment modes excluded).
inline Ui::IconButton *telegramSendButton(QWidget *parent) {
    ensureTelegramUiRuntime();
    return new Ui::IconButton(parent, st::lanHistorySend);
}
