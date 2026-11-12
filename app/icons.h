#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

QPixmap loadTintedIcon(const QString &resource, const QColor &color, int size);
QIcon loadTintedQIcon(const QString &resource, const QColor &color, int size);
void paintHamburger(class QPainter *painter, const QRect &rect, const QColor &color);
void paintPlane(class QPainter *painter, const QRect &rect, const QColor &color);
