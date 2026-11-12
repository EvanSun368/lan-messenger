#pragma once

#include "models.h"

#include <QRect>
#include <QString>

struct FileBubbleMetrics {
	int maxInnerWidth = 280;
	int iconSize = 40;
	int iconGap = 10;
	int fileRowHeight = 48;
	int buttonHeight = 26;
	int buttonGap = 8;
	int rowGap = 8;
	int acceptWidth = 56;
	int declineWidth = 56;
	int saveAsWidth = 120;
	int timeWidth = 36;
	int timeHeight = 16;
	bool incomingOffer = false;
	bool verifiedComplete = false;
	int captionHeight = 0;
};

struct FileBubbleLayout {
	int width = 0;
	int height = 0;
	QRect icon;
	QRect name;
	QRect status;
	QRect accept;
	QRect decline;
	QRect saveAs;
	QRect time;
};

// Positions are relative to the content origin (inside bubble padding).
// Buttons wrap when they would overflow, and the time sits on its own
// reserved row so it cannot collide with the filename or actions.
FileBubbleLayout layout_file_bubble(const FileBubbleMetrics &metrics);

// Localized status line under the filename (waiting / progress / declined).
QString file_transfer_status(const ChatMessage &msg);
