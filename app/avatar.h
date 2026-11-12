#pragma once

#include <QPixmap>
#include <QString>

QPixmap makeAvatar(
	const QString &id,
	const QString &name,
	int size,
	bool selected = false,
	const QString &photoPath = {});
void setCachedAvatar(const QString &id, const QString &photoPath);
QString cachedAvatarPath(const QString &id);
