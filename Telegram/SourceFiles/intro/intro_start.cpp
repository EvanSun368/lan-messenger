/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "intro/intro_start.h"

#include "lan/bridge.h"
#include "lan/identity.h"
#include "lan/lan_mode.h"

#include "intro/intro_widget.h"
#include "lang/lang_keys.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "main/main_account.h"
#include "styles/style_intro.h"

namespace Intro {
namespace details {

StartWidget::StartWidget(
	QWidget *parent,
	not_null<Main::Account*> account,
	not_null<Data*> data)
: Step(parent, account, data, true)
, _name(this, st::introName, rpl::single(tr::lng_signup_firstname(tr::now))) {
	setMouseTracking(true);
	setTitleText(rpl::single(QString::fromUtf8(Lan::kProductName)));
	setDescriptionText(rpl::single(
		u"Start with a local display name on your LAN. "
		"This app will not log into Telegram.\n"
		"用本地显示名在局域网里开始，不会登录 Telegram 云。"_q));
	if (const auto existing = Lan::TryLoadIdentity()) {
		_name->setText(QString::fromUtf8(
			existing->display_name.data(),
			int(existing->display_name.size())));
	}
	_name->submits(
	) | rpl::on_next([=] { submit(); }, _name->lifetime());
	show();
}

void StartWidget::submit() {
	hideError();
	const auto typed = _name->getLastText().toUtf8();
	const auto normalized = lan::normalize_display_name(
		std::string_view(typed.constData(), size_t(typed.size())));
	if (normalized.empty()) {
		showError(rpl::single(
			u"Enter a display name (1-64 characters).\n"
			"请输入显示名（1 到 64 个字符）。"_q));
		setInnerFocus();
		return;
	}
	try {
		const auto identity = Lan::SaveOrUpdateIdentity(normalized);
		const auto account = &this->account();
		// createSession destroys the intro stack, including this widget.
		Lan::StartLocalSession(account, identity);
	} catch (const std::exception &error) {
		showError(rpl::single(QString::fromUtf8(error.what())));
		setInnerFocus();
	}
}

rpl::producer<QString> StartWidget::nextButtonText() const {
	return rpl::single(u"Start"_q);
}

rpl::producer<> StartWidget::nextButtonFocusRequests() const {
	return _nextButtonFocusRequests.events();
}

void StartWidget::activate() {
	Step::activate();
	_name->show();
	setInnerFocus();
}

void StartWidget::setInnerFocus() {
	_name->setFocusFast();
}

void StartWidget::resizeEvent(QResizeEvent *e) {
	Step::resizeEvent(e);
	updateControlsGeometry();
}

void StartWidget::updateControlsGeometry() {
	const auto top = contentTop() + st::introStepFieldTop;
	_name->moveToLeft(contentLeft(), top);
}

} // namespace details
} // namespace Intro
