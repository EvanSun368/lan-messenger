/* Source provenance and licence: telegram_input_logic.h */
#include "telegram_input_logic.h"
#include <algorithm>
namespace TelegramInputLogic {
void InstantReplaces::add(const QString &what, const QString &with) {
	auto node = &reverseMap;
	for (auto i = what.end(), b = what.begin(); i != b;) {
		node = &node->tail.emplace(*--i, Node()).first->second;
	}
	node->text = with;
	maxLength = std::max(maxLength, int(what.size()));
}
const InstantReplaces &InstantReplaces::TextOnly() {
	static const auto result = [] {
		auto result = InstantReplaces();
		result.add("--", QString(1, QChar(8212)));
		result.add("<<", QString(1, QChar(171)));
		result.add(">>", QString(1, QChar(187)));
		result.add(
			":shrug:",
			QChar(175) + QString("\\_(") + QChar(12484) + ")_/" + QChar(175));
		return result;
	}();
	return result;
}
bool ShouldSubmit(
		SubmitSettings settings,
		Qt::KeyboardModifiers modifiers) {
	const auto shift = modifiers.testFlag(Qt::ShiftModifier);
	const auto ctrl = modifiers.testFlag(Qt::ControlModifier)
		|| modifiers.testFlag(Qt::MetaModifier);
	return (ctrl && shift)
		|| (ctrl
			&& settings != SubmitSettings::None
			&& settings != SubmitSettings::Enter)
		|| (!ctrl
			&& !shift
			&& settings != SubmitSettings::None
			&& settings != SubmitSettings::CtrlEnter);
}

Replacement FindReplacement(const InstantReplaces &replaces,
        const QString &beforeCursor,const QString &appended) {
    // Adapted processInstantReplaces: supplied plain-text range replaces the
    // lib_ui tagged-document access. Caller excludes code spans and paste/IME.
    if (appended.size() != 1 || !replaces.maxLength) return {};
    const auto it = replaces.reverseMap.tail.find(appended[0]);
    if (it == end(replaces.reverseMap.tail)) return {};
    const auto typed = beforeCursor.right(replaces.maxLength - 1);
    auto node = &it->second;
    auto i = typed.size();
    do {
        if (!node->text.isEmpty()) {
            return {int(typed.size()-i+appended.size()),node->text};
        } else if (!i) {
            return {};
        }
        const auto it = node->tail.find(typed[--i]);
        if (it == end(node->tail)) return {};
        node = &it->second;
    } while (true);
}

}
