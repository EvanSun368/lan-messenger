#include "check.h"
#include "i18n.h"

#include <QString>
#include <string>

namespace {

std::string s(const QString &text) {
	return text.toStdString();
}

} // namespace

LAN_TEST(i18n_waiting_reject_recall_follow_language) {
	i18n::setLanguage(Language::ChineseSimplified);
	LAN_CHECK_EQ(s(i18n::tr("waiting_to_accept")), std::string("等待接收"));
	LAN_CHECK_EQ(s(i18n::tr("rejected")), std::string("拒绝接收"));
	LAN_CHECK_EQ(s(i18n::tr("recall")), std::string("撤回"));
	LAN_CHECK_EQ(s(i18n::tr("version").arg(QStringLiteral("0.9"))), std::string("版本 0.9"));

	i18n::setLanguage(Language::ChineseTraditional);
	LAN_CHECK_EQ(s(i18n::tr("waiting_to_accept")), std::string("等待接收"));
	LAN_CHECK_EQ(s(i18n::tr("rejected")), std::string("拒絕接收"));
	LAN_CHECK_EQ(s(i18n::tr("recall")), std::string("撤回"));
	LAN_CHECK_EQ(s(i18n::tr("version").arg(QStringLiteral("0.9"))), std::string("版本 0.9"));

	i18n::setLanguage(Language::English);
	LAN_CHECK_EQ(s(i18n::tr("waiting_to_accept")), std::string("Waiting to accept"));
	LAN_CHECK_EQ(s(i18n::tr("rejected")), std::string("Declined to receive"));
	LAN_CHECK_EQ(s(i18n::tr("recall")), std::string("Recall"));
	LAN_CHECK_EQ(s(i18n::tr("version").arg(QStringLiteral("0.9"))), std::string("Version 0.9"));
}

LAN_TEST(i18n_localize_rewrites_stored_and_legacy_strings) {
	i18n::setLanguage(Language::ChineseSimplified);
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("rejected"))), std::string("拒绝接收"));
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("Declined to receive"))), std::string("拒绝接收"));
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("declined"))), std::string("拒绝接收"));
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("拒絕接收"))), std::string("拒绝接收"));
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("waiting_to_accept"))), std::string("等待接收"));
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("recall"))), std::string("撤回"));
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("peer aborted the transfer"))), std::string("已取消"));

	i18n::setLanguage(Language::English);
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("拒绝接收"))), std::string("Declined to receive"));
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("撤回"))), std::string("Recall"));

	i18n::setLanguage(Language::ChineseTraditional);
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("Declined to receive"))), std::string("拒絕接收"));
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("拒绝接收"))), std::string("拒絕接收"));
}

LAN_TEST(i18n_unknown_value_is_unchanged) {
	i18n::setLanguage(Language::English);
	LAN_CHECK_EQ(s(i18n::localize(QStringLiteral("disk full"))), std::string("disk full"));
}
