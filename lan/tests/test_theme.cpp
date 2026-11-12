#include "check.h"
#include "theme.h"

#include <QString>
#include <string>

namespace {

std::string s(const QString &text) {
	return text.toStdString();
}

} // namespace

LAN_TEST(theme_day_and_night_palettes_are_cached) {
	const Theme &day1 = Theme::light();
	const Theme &day2 = Theme::light();
	const Theme &night1 = Theme::night();
	const Theme &night2 = Theme::night();
	LAN_CHECK(&day1 == &day2);
	LAN_CHECK(&night1 == &night2);
	LAN_CHECK(&day1 != &night1);
}

LAN_TEST(theme_official_cover_and_window_colors) {
	const auto &day = Theme::light();
	LAN_CHECK(!day.dark);
	LAN_CHECK_EQ(s(day.dialogsBgActive.name()), std::string("#419fd9"));
	LAN_CHECK_EQ(s(day.windowBg.name()), std::string("#ffffff"));
	LAN_CHECK_EQ(s(day.composeBg.name()), std::string("#ffffff"));
	LAN_CHECK_EQ(s(day.boxBg.name()), std::string("#ffffff"));

	const auto &night = Theme::night();
	LAN_CHECK(night.dark);
	LAN_CHECK_EQ(s(night.dialogsBgActive.name()), std::string("#2b5278"));
	LAN_CHECK_EQ(s(night.windowBg.name()), std::string("#0e1621"));
	LAN_CHECK_EQ(s(night.composeBg.name()), std::string("#17212b"));
	LAN_CHECK_EQ(s(night.boxBg.name()), std::string("#17212b"));
	LAN_CHECK_EQ(s(night.topBarBg.name()), std::string("#17212b"));
}

LAN_TEST(theme_set_current_switches_cached_palette) {
	const Theme &night = Theme::night();
	Theme::setCurrent(night);
	LAN_CHECK(Theme::current().dark);
	LAN_CHECK(&Theme::current() == &night);
	LAN_CHECK_EQ(s(Theme::current().dialogsBgActive.name()), std::string("#2b5278"));

	const Theme &day = Theme::light();
	Theme::setCurrent(day);
	LAN_CHECK(!Theme::current().dark);
	LAN_CHECK(&Theme::current() == &day);
	LAN_CHECK_EQ(s(Theme::current().dialogsBgActive.name()), std::string("#419fd9"));

	const Theme *before = &Theme::current();
	Theme::setCurrent(Theme::light());
	LAN_CHECK(&Theme::current() == before);
}
