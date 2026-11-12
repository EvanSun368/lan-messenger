#include "file_bubble.h"

#include "i18n.h"

#include <QtGlobal>

FileBubbleLayout layout_file_bubble(const FileBubbleMetrics &metrics) {
	FileBubbleLayout out;
	const int icon = qMax(24, metrics.iconSize);
	const int gap = qMax(4, metrics.iconGap);
	const int fileH = qMax(icon + 8, metrics.fileRowHeight);
	const int btnH = qMax(18, metrics.buttonHeight);
	const int btnGap = qMax(4, metrics.buttonGap);
	const int rowGap = qMax(4, metrics.rowGap);
	const int timeH = qMax(12, metrics.timeHeight);
	const int timeW = qMax(24, metrics.timeWidth);

	// Use the caller's inner width so a long filename or the three
	// offer actions can spread out. If that width is tight, buttons wrap.
	out.width = metrics.maxInnerWidth > 0 ? metrics.maxInnerWidth : (icon + gap + 80);

	out.icon = QRect(0, 4, icon, icon);
	const int textX = icon + gap;
	const int textW = qMax(24, out.width - textX);
	out.name = QRect(textX, 4, textW, 20);
	out.status = QRect(textX, 24, textW, 18);

	int y = fileH;
	if (metrics.incomingOffer) {
		y += rowGap;
		const int acceptW = qMax(24, metrics.acceptWidth);
		const int declineW = qMax(24, metrics.declineWidth);
		const int saveW = qMax(24, metrics.saveAsWidth);
		out.accept = QRect(0, y, qMin(acceptW, out.width), btnH);
		int declineX = out.accept.width() + btnGap;
		if (declineX + declineW <= out.width) {
			out.decline = QRect(declineX, y, declineW, btnH);
		} else {
			y += btnH + rowGap;
			out.decline = QRect(0, y, qMin(declineW, out.width), btnH);
		}
		int saveX = out.decline.right() + 1 + btnGap;
		if (saveX + saveW <= out.width && out.decline.y() == y) {
			out.saveAs = QRect(saveX, y, saveW, btnH);
			y += btnH;
		} else {
			y += btnH + rowGap;
			out.saveAs = QRect(0, y, qMin(saveW, out.width), btnH);
			y += btnH;
		}
	}

	if (metrics.captionHeight > 0) {
		y += 4 + metrics.captionHeight;
	}
	if (metrics.verifiedComplete) {
		y += 2;
	}
	y += rowGap;
	out.time = QRect(out.width - timeW, y, timeW, timeH);
	y += timeH;
	out.height = y;
	return out;
}

QString file_transfer_status(const ChatMessage &msg) {
	if (msg.failed) {
		if (msg.error.isEmpty()) {
			return i18n::tr("failed");
		}
		return i18n::localize(msg.error);
	}
	if (msg.pendingAccept && msg.outgoing) {
		return i18n::tr("waiting_to_accept");
	}
	if (msg.pendingAccept && !msg.outgoing) {
		return format_size(msg.size);
	}
	if (!msg.complete && msg.size > 0) {
		return format_size(msg.bytesDone) + QStringLiteral(" / ") + format_size(msg.size);
	}
	if (msg.complete && msg.verified) {
		return format_size(msg.size) + QStringLiteral(" · ") + i18n::tr("verified");
	}
	if (msg.size > 0) {
		return format_size(msg.size);
	}
	return {};
}
