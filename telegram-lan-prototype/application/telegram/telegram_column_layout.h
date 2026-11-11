/*
 * Derived from Telegram Desktop SessionController::computeColumnLayout and
 * countDialogsWidthFromRatio, window/window_session_controller.cpp.
 * Copyright (c) The Telegram Desktop Authors. GPL-3.0-or-later, OpenSSL exception.
 * Source commit c4bd064bcc84a359179e191fa6dfd216acda504f.
 * Session/settings/third column removed; explicit width/ratio snapshot replaces them.
 */
#pragma once
#include <algorithm>
#include <cmath>
namespace TelegramLan {
struct ColumnLayout { int bodyWidth, dialogsWidth, chatWidth; bool oneColumn; };
inline ColumnLayout ComputeColumnLayout(int bodyWidth, double dialogsRatio=0.32) {
    // window/window.style: columnMinimalWidthLeft/Main.
    constexpr int leftMinimum=260, mainMinimum=380;
    bodyWidth=std::max(0,bodyWidth);
    if(bodyWidth<leftMinimum+mainMinimum) return {bodyWidth,bodyWidth,bodyWidth,true};
    auto dialogsWidth=std::max(int(std::lround(bodyWidth*dialogsRatio)),leftMinimum);
    dialogsWidth=std::min(dialogsWidth,bodyWidth-mainMinimum);
    return {bodyWidth,dialogsWidth,bodyWidth-dialogsWidth,false};
}
}
