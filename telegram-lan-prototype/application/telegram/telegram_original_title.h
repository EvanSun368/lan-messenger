#pragma once
// Host-only construction helper. The title painting, system control buttons and
// window drag/maximize behavior are the unchanged upstream lib_ui implementation.
#include "ui/ui_pch.h"
#include "ui/rp_widget.h"
#include "ui/platform/ui_platform_window_title.h"
#include "styles/style_widgets.h"
void ensureTelegramUiRuntime();
inline Ui::Platform::DefaultTitleWidget *telegramWindowTitle(
        Ui::RpWidget *parent, const QString &text) {
    ensureTelegramUiRuntime();
    auto *title = new Ui::Platform::DefaultTitleWidget(parent);
    title->setText(text);
    title->setFixedHeight(title->st()->height);
    title->setObjectName("telegramOriginalTitle");
    return title;
}
