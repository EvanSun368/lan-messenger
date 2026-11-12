#pragma once

#include "dialog_merge.h"
#include "models.h"

#include <filesystem>
#include <QHash>
#include <QStringList>
#include <QVector>

class HistoryStore {
public:
	explicit HistoryStore(std::filesystem::path dir);

	QVector<ChatMessage> load(const QString &chatId) const;
	void save(const QString &chatId, const QVector<ChatMessage> &messages) const;
	QStringList chatIds() const;
	void remove(const QString &chatId) const;
	QVector<PeerHint> loadHints() const;
	void saveHints(const QVector<PeerHint> &hints) const;

private:
	std::filesystem::path dir_;
};
