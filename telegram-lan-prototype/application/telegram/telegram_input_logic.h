/* Adapted from desktop-app/lib_ui at 42c6f1e88db81e6d9e8006f3e7b1c3ba39f9cac4
 * ui/widgets/fields/input_field.{h,cpp}, used by Telegram Desktop 7.4.1.
 * Copyright (c) 2014-2026 The Desktop App Toolkit Authors.
 * GPL-3.0-or-later; see DESKTOP_APP_LEGAL and GPL LICENSE in parent project.
 * Extracted: InstantReplaces trie, TextOnly map, submit modifier predicate,
 * and reverse-trie lookup. Not the complete Ui::InputField implementation.
 */
#pragma once
#include <QString>
#include <Qt>
#include <map>
namespace TelegramInputLogic {
enum class SubmitSettings { Enter, CtrlEnter, Both, None };
struct InstantReplaces {
 struct Node { QString text; std::map<QChar,Node> tail; };
 void add(const QString &what,const QString &with);
 static const InstantReplaces &TextOnly();
 int maxLength=0; Node reverseMap;
};
bool ShouldSubmit(SubmitSettings settings,Qt::KeyboardModifiers modifiers);
struct Replacement { int length=0; QString text; };
Replacement FindReplacement(const InstantReplaces &replaces,const QString &beforeCursor,const QString &appended);
}
