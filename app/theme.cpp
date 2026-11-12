#include "theme.h"

#include <QApplication>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QPixmap>
#include <QPainter>

namespace {

Theme g_light;
Theme g_night;
const Theme *g_current = nullptr;
bool g_fontsLoaded = false;
bool g_palettesReady = false;
QPalette g_lightPalette;
QPalette g_nightPalette;

void fillPalette(QPalette &palette, const Theme &theme) {
	palette.setColor(QPalette::Window, theme.boxBg);
	palette.setColor(QPalette::WindowText, theme.windowFg);
	palette.setColor(QPalette::Base, theme.boxBg);
	palette.setColor(QPalette::AlternateBase, theme.windowBgOver);
	palette.setColor(QPalette::Text, theme.windowFg);
	palette.setColor(QPalette::Button, theme.windowBgOver);
	palette.setColor(QPalette::ButtonText, theme.windowFg);
	palette.setColor(QPalette::Highlight, theme.windowBgActive);
	palette.setColor(QPalette::HighlightedText, theme.windowFgActive);
	palette.setColor(QPalette::ToolTipBase, theme.boxBg);
	palette.setColor(QPalette::ToolTipText, theme.windowFg);
	palette.setColor(QPalette::PlaceholderText, theme.placeholderFg);
	palette.setColor(QPalette::Light, theme.windowBgOver);
	palette.setColor(QPalette::Mid, theme.inputBorderFg);
	palette.setColor(QPalette::Dark, theme.windowSubTextFg);
}


QStringList fontFamilies() {
	return {
		QStringLiteral("Open Sans"),
		QStringLiteral("Noto Sans CJK SC"),
		QStringLiteral("Noto Sans CJK TC"),
		QStringLiteral("Microsoft YaHei UI"),
		QStringLiteral("Microsoft YaHei"),
		QStringLiteral("PingFang SC"),
		QStringLiteral("PingFang TC"),
		QStringLiteral("Source Han Sans SC"),
		QStringLiteral("WenQuanYi Micro Hei"),
		QStringLiteral("Noto Sans"),
		QStringLiteral("Sans Serif"),
	};
}

QFont makeFont(int point, QFont::Weight weight = QFont::Normal) {
	QFont font;
	font.setFamilies(fontFamilies());
	font.setPointSize(point);
	font.setWeight(weight);
	font.setStyleHint(QFont::SansSerif);
	font.setStyleStrategy(QFont::PreferAntialias);
	return font;
}

void applyFonts(Theme &theme) {
	theme.fontName = makeFont(14, QFont::DemiBold);
	theme.fontText = makeFont(13);
	theme.fontPreview = makeFont(13);
	theme.fontDate = makeFont(12);
	theme.fontBadge = makeFont(11, QFont::DemiBold);
	theme.fontHeader = makeFont(14, QFont::DemiBold);
	theme.fontStatus = makeFont(12);
	theme.fontService = makeFont(12);
	theme.fontUserpic = makeFont(18, QFont::DemiBold);
	theme.fontBoxTitle = makeFont(16, QFont::DemiBold);
	theme.fontBoxButton = makeFont(14, QFont::DemiBold);
	theme.fontMenuItem = makeFont(14, QFont::DemiBold);
	QFont mono;
	mono.setFamilies({
		QStringLiteral("Cascadia Mono"),
		QStringLiteral("Consolas"),
		QStringLiteral("Menlo"),
		QStringLiteral("DejaVu Sans Mono"),
		QStringLiteral("Noto Sans Mono"),
		QStringLiteral("Courier New"),
		QStringLiteral("Monospace"),
	});
	mono.setPointSize(12);
	mono.setStyleHint(QFont::Monospace);
	mono.setStyleStrategy(QFont::PreferAntialias);
	theme.fontMono = mono;
}

} // namespace

void fillNightColors(Theme &theme) {
	theme.dark = true;
	theme.windowBg = QColor("#0e1621");
	theme.windowFg = QColor("#f5f5f5");
	theme.windowBgOver = QColor("#17212b");
	theme.windowBgRipple = QColor("#202b36");
	theme.windowBgActive = QColor("#2b5278");
	theme.windowFgActive = QColor("#ffffff");
	theme.windowActiveTextFg = QColor("#6ab2f2");
	theme.windowBoldFg = QColor("#f5f5f5");
	theme.windowSubTextFg = QColor("#708499");
	theme.windowSubTextFgOver = QColor("#7d8e9e");
	theme.dialogsBg = QColor("#0e1621");
	theme.dialogsBgOver = QColor("#17212b");
	theme.dialogsBgActive = QColor("#2b5278");
	theme.dialogsNameFg = QColor("#f5f5f5");
	theme.dialogsNameFgActive = QColor("#ffffff");
	theme.dialogsTextFg = QColor("#708499");
	theme.dialogsTextFgActive = QColor("#ffffff");
	theme.dialogsDateFg = QColor("#708499");
	theme.dialogsDateFgActive = QColor("#ffffff");
	theme.dialogsUnreadBg = QColor("#3e70a4");
	theme.dialogsUnreadBgMuted = QColor("#3e546a");
	theme.dialogsDivider = QColor("#0e1621");
	theme.topBarBg = QColor("#17212b");
	theme.msgInBg = QColor("#182533");
	theme.msgOutBg = QColor("#2b5278");
	theme.msgInDateFg = QColor("#6d7f8f");
	theme.msgOutDateFg = QColor("#7aa3c4");
	theme.historyTextInFg = QColor("#f5f5f5");
	theme.historyTextOutFg = QColor("#f5f5f5");
	theme.historyOutIconFg = QColor("#72b5e8");
	theme.historySendingOutIconFg = QColor("#5b8cb0");
	theme.historyLinkFg = QColor("#6ab2f2");
	theme.composeBg = QColor("#17212b");
	theme.composeFg = QColor("#f5f5f5");
	theme.composeIconFg = QColor("#6c7883");
	theme.sendIconFg = QColor("#5288c1");
	theme.placeholderFg = QColor("#6c7883");
	theme.inputBorderFg = QColor("#101921");
	theme.filterInputBg = QColor("#242f3d");
	theme.menuIconFg = QColor("#6c7883");
	theme.sideBarBg = QColor("#17212b");
	theme.boxBg = QColor("#17212b");
	theme.boxTitleFg = QColor("#f5f5f5");
	theme.onlineFg = QColor("#6ab3f3");
	theme.lightButtonBgOver = QColor("#1c2b38");
	theme.boxTitleCloseFg = QColor("#6c7883");
	theme.historyCodeBg = QColor(255, 255, 255, 18);
	theme.mainMenuCoverFg = QColor("#ffffff");
}

void ensurePalettes() {
	if (g_palettesReady) {
		return;
	}
	g_light = Theme();
	applyFonts(g_light);
	g_night = Theme();
	fillNightColors(g_night);
	applyFonts(g_night);
	fillPalette(g_lightPalette, g_light);
	fillPalette(g_nightPalette, g_night);
	if (!g_current) {
		g_current = &g_light;
	}
	g_palettesReady = true;
}

const Theme &Theme::current() {
	ensurePalettes();
	return *g_current;
}

ThemeNotifier &Theme::notifier() {
	static ThemeNotifier instance;
	return instance;
}

void Theme::applyApplicationPalette() {
	// Official Telegram restyles in place via palette notify, not a
	// process-wide stylesheet. setStyleSheet() on QApplication forces a
	// full polish of the widget tree and is what made the night toggle hitch.
	ensurePalettes();
	if (auto *app = qApp) {
		if (app->styleSheet().isEmpty() == false) {
			app->setStyleSheet(QString());
		}
		app->setPalette(g_current && g_current->dark ? g_nightPalette : g_lightPalette);
	}
}

void Theme::setCurrent(const Theme &theme) {
	ensurePalettes();
	const Theme *next = theme.dark ? &g_night : &g_light;
	const bool changed = (g_current != next);
	g_current = next;
	applyApplicationPalette();
	if (changed) {
		emit notifier().changed();
	}
}

const Theme &Theme::light() {
	ensurePalettes();
	return g_light;
}

const Theme &Theme::night() {
	ensurePalettes();
	return g_night;
}

void Theme::loadFonts() {
	if (g_fontsLoaded) {
		return;
	}
	const QStringList files = {
		QStringLiteral(":/fonts/OpenSans-Regular.ttf"),
		QStringLiteral(":/fonts/OpenSans-SemiBold.ttf"),
		QStringLiteral(":/fonts/OpenSans-Italic.ttf"),
		QStringLiteral(":/fonts/OpenSans-SemiBoldItalic.ttf"),
	};
	for (const auto &file : files) {
		QFontDatabase::addApplicationFont(file);
	}
	g_fontsLoaded = true;
	if (g_palettesReady) {
		applyFonts(g_light);
		applyFonts(g_night);
	}
}

int Theme::userpicIndex(const QString &id) {
	uint hash = 0;
	for (const QChar ch : id) {
		hash = hash * 16777619u ^ uint(ch.unicode());
	}
	return int(hash % 8);
}

QString Theme::initialLetter(const QString &name) {
	for (const QChar ch : name.trimmed()) {
		if (ch.isLetterOrNumber()) {
			return QString(ch).toUpper();
		}
	}
	return QStringLiteral("L");
}


const QPixmap &Theme::wallpaper() {
	static QPixmap day;
	static QPixmap night;
	if (day.isNull()) {
		day = QPixmap(QStringLiteral(":/art/chat_background.jpg"));
	}
	ensurePalettes();
	if (!g_current->dark) {
		return day;
	}
	if (night.isNull() && !day.isNull()) {
		night = day;
		QPainter painter(&night);
		painter.fillRect(night.rect(), QColor(0, 0, 0, 80));
	}
	return night.isNull() ? day : night;
}
