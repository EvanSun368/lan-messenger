/*
 * Adapted from Telegram Desktop dialogs/ui/dialogs_layout.cpp (PaintRow,
 * PaintRowTopRight, PaintWideCounter) and ui/unread_badge_paint.cpp.
 * Copyright (c) 2014-2026 The Telegram Desktop Authors.
 * GPL-3.0-or-later with upstream OpenSSL exception; see LICENSE/LEGAL.
 * Upstream: c4bd064bcc84a359179e191fa6dfd216acda504f.
 * LAN adapter: immutable presentation replaces Entry/Session/History data,
 * QPainter/QFontMetrics replace lib_ui Painter/style font wrappers.
 */
#pragma once
#include <QColor>
#include <QImage>
#include <QFont>
#include <QPainter>
#include <QRect>
namespace TelegramLan {
struct DialogPresentation {
 QString id,name,preview,date;
 QImage avatar;
 QColor avatarColor=QColor("#419fd9");
 int unread=0;
 bool muted=false,online=false;
};
struct DialogPaintContext {
 int width=320;
 bool active=false,selected=false,narrow=false;
};
struct DialogPaintResult {
 QRect avatar,name,preview,date,badge;
};
// Style values originate from defaultDialogRow/dialogs.style and colors.palette.
inline constexpr int TelegramDialogRowHeight=62;
DialogPaintResult PaintTelegramDialogRow(QPainter &p,const DialogPresentation &row,
 const DialogPaintContext &context);
}
