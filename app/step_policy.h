#pragma once

#include "dialog_merge.h"
#include "models.h"

#include <QVector>

// Idle ticks stay at 40 ms. While bytes are actually moving, drain for a
// few milliseconds and wake on a short, non-zero interval so the UI
// thread and the LAN are not starved by QTimer(0).
inline constexpr int kIdleStepTimerMs = 40;
inline constexpr int kSpinStepTimerMs = 4;
inline constexpr int kSpinDrainMs = 4;
inline constexpr qint64 kTransferUiMinIntervalMs = 100;

inline int step_timer_interval_ms(bool needsSpin) {
	return needsSpin ? kSpinStepTimerMs : kIdleStepTimerMs;
}

inline bool peer_hints_equal(const QVector<PeerHint> &a, const QVector<PeerHint> &b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (int i = 0; i < a.size(); ++i) {
		if (a[i].id != b[i].id || a[i].name != b[i].name || a[i].ip != b[i].ip) {
			return false;
		}
	}
	return true;
}

// Offered / Started / Completed / Failed (including a decline) always
// refresh the bubble. Progress is published at most every 100 ms, plus
// the first progress event.
inline bool should_publish_transfer_ui(
		bool isProgress,
		qint64 lastPublishMs,
		qint64 nowMs,
		qint64 minIntervalMs = kTransferUiMinIntervalMs) {
	if (!isProgress) {
		return true;
	}
	if (lastPublishMs <= 0) {
		return true;
	}
	return nowMs - lastPublishMs >= minIntervalMs;
}

struct PeerUpsertChange {
	bool created = false;
	bool hintChanged = false;
	bool dialogChanged = false;
	bool cameOnline = false;
	bool identityBecameMergeable = false;
};

inline bool should_merge_offline_leftovers(const PeerUpsertChange &change, bool online) {
	return online
		&& (change.created || change.cameOnline || change.identityBecameMergeable);
}

inline PeerUpsertChange apply_peer_upsert(
		Dialog &item,
		const QString &name,
		bool online,
		const QString &ip,
		const QString &firstName,
		const QString &lastName,
		const QString &bio) {
	PeerUpsertChange change;
	const auto oldName = item.name;
	const auto oldIp = item.ip;
	const bool wasOnline = item.online;
	const bool nameWasUsable = is_usable_display_name(item.id, oldName);

	if (!name.isEmpty() && item.name != name) {
		item.name = name;
		change.dialogChanged = true;
		change.hintChanged = true;
	}
	if (item.online != online) {
		item.online = online;
		change.dialogChanged = true;
		if (online && !wasOnline) {
			change.cameOnline = true;
		}
	}
	if (!ip.isEmpty() && item.ip != ip) {
		item.ip = ip;
		change.dialogChanged = true;
		change.hintChanged = true;
	}
	if (!firstName.isEmpty() || !lastName.isEmpty()) {
		if (item.firstName != firstName || item.lastName != lastName) {
			item.firstName = firstName;
			item.lastName = lastName;
			change.dialogChanged = true;
		}
	}
	if (!bio.isEmpty() || !firstName.isEmpty() || !lastName.isEmpty()) {
		if (item.bio != bio) {
			item.bio = bio;
			change.dialogChanged = true;
		}
	}
	if (oldIp.isEmpty() && !item.ip.isEmpty()) {
		change.identityBecameMergeable = true;
	}
	if (!nameWasUsable && is_usable_display_name(item.id, item.name)) {
		change.identityBecameMergeable = true;
	}
	return change;
}
