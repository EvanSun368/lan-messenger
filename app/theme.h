#pragma once

#include <QColor>
#include <QObject>
#include <QFont>
#include <QString>

class ThemeNotifier : public QObject {
	Q_OBJECT
signals:
	void changed();
};

// Official Telegram Desktop light (and optional night) color tokens,
// taken from lib_ui/ui/colors.palette. Brand remains LanLink.
struct Theme {
	bool dark = false;

	QColor windowBg = QColor("#ffffff");
	QColor windowFg = QColor("#000000");
	QColor windowBgOver = QColor("#f1f1f1");
	QColor windowBgRipple = QColor("#e5e5e5");
	QColor windowBgActive = QColor("#40a7e3");
	QColor windowFgActive = QColor("#ffffff");
	QColor windowActiveTextFg = QColor("#168acd");
	QColor windowBoldFg = QColor("#222222");
	QColor windowSubTextFg = QColor("#999999");
	QColor windowSubTextFgOver = QColor("#919191");

	QColor dialogsBg = QColor("#ffffff");
	QColor dialogsBgOver = QColor("#f1f1f1");
	QColor dialogsBgActive = QColor("#419fd9");
	QColor dialogsNameFg = QColor("#222222");
	QColor dialogsNameFgActive = QColor("#ffffff");
	QColor dialogsTextFg = QColor("#999999");
	QColor dialogsTextFgActive = QColor("#ffffff");
	QColor dialogsDateFg = QColor("#999999");
	QColor dialogsDateFgActive = QColor("#ffffff");
	QColor dialogsUnreadBg = QColor("#40a7e3");
	QColor dialogsUnreadBgMuted = QColor("#bbbbbb");
	QColor dialogsUnreadFg = QColor("#ffffff");
	QColor dialogsDivider = QColor("#e7e7e7");

	QColor topBarBg = QColor("#ffffff");
	QColor topBarShadow = QColor(0, 0, 0, 18);

	QColor msgInBg = QColor("#ffffff");
	QColor msgOutBg = QColor("#effdde");
	QColor msgInDateFg = QColor("#a0acb6");
	QColor msgOutDateFg = QColor("#6db566");
	QColor msgInShadow = QColor(0x74, 0x8e, 0xa2, 0x29);
	QColor msgOutShadow = QColor(0x3a, 0xc3, 0x46, 0x1d);
	QColor msgServiceFg = QColor("#ffffff");
	QColor msgServiceBg = QColor(0x51, 0x7c, 0x41, 0x7f);
	QColor historyTextInFg = QColor("#000000");
	QColor historyTextOutFg = QColor("#000000");
	QColor historyOutIconFg = QColor("#57b84c");
	QColor historySendingOutIconFg = QColor("#98d292");
	QColor historyLinkFg = QColor("#168acd");
	QColor historyFile1Bg = QColor("#72b1df");
	QColor historyFile2Bg = QColor("#5fbe67");
	QColor historyFile3Bg = QColor("#e47272");
	QColor historyFile4Bg = QColor("#efc274");

	QColor composeBg = QColor("#ffffff");
	QColor composeFg = QColor("#000000");
	QColor composeIconFg = QColor("#999999");
	QColor composeIconFgOver = QColor("#8a8a8a");
	QColor sendIconFg = QColor("#40a7e3");
	QColor placeholderFg = QColor("#999999");
	QColor inputBorderFg = QColor("#e0e0e0");
	QColor filterInputBg = QColor("#f1f1f1");
	QColor menuIconFg = QColor("#999999");
	// Official lib_ui/ui/colors.palette layerBg: dim behind the main menu.
	QColor layerBg = QColor(0, 0, 0, 0x7f);
	QColor lightButtonBgOver = QColor("#e3f1fa");
	QColor boxTitleCloseFg = QColor("#999999");
	QColor historyCodeBg = QColor(0, 0, 0, 16);
	QColor mainMenuCoverFg = QColor("#ffffff");

	QColor sideBarBg = QColor("#293a4c");
	QColor sideBarFg = QColor("#ffffff");
	QColor boxTitleFg = QColor("#404040");
	QColor boxBg = QColor("#ffffff");
	QColor attentionFg = QColor("#d14e4e");
	QColor onlineFg = QColor("#168acd");

	QColor userpicFg = QColor("#ffffff");
	QColor userpic[8] = {
		QColor("#ff845e"),
		QColor("#9ad164"),
		QColor("#e5ca77"),
		QColor("#5caffa"),
		QColor("#b694f9"),
		QColor("#ff8aac"),
		QColor("#5bcbe3"),
		QColor("#febb5b"),
	};
	QColor peerName[8] = {
		QColor("#c03d33"),
		QColor("#4fad2d"),
		QColor("#d09306"),
		QColor("#168acd"),
		QColor("#8544d6"),
		QColor("#cd4073"),
		QColor("#2996ad"),
		QColor("#ce671b"),
	};

	int dialogsWidth = 350;
	int dialogsRowHeight = 72;
	int dialogsPhoto = 48;
	int topBarHeight = 56;
	int composeMinHeight = 52;
	// Official tdesktop historyComposeFieldMaxHeight.
	int composeFieldMaxHeight = 224;
	// Official historyComposeField heightMin; heightMax in the style
	// is 72, but the widget calls setMaxHeight(historyComposeFieldMaxHeight)
	// which is 224.
	int composeFieldMinHeight = 36;
	// Official Desktop shows history text in full because Telegram caps
	// a message at 4096 characters (~50 lines). LanLink allows up to
	// 1 MiB, so we keep the same on-screen budget and offer "Show more".
	int historyTextMaxLines = 50;
	int bubbleRadius = 14;
	int bubbleTail = 6;
	int avatarInGroup = 32;
	int mainMenuWidth = 274;
	int mainMenuCoverHeight = 134;
	int mainMenuUserpic = 48;
	int mainMenuUserpicLeft = 24;
	int mainMenuUserpicTop = 20;
	int mainMenuCoverNameLeft = 26;
	int mainMenuCoverNameTop = 84;
	int mainMenuCoverStatusTop = 103;
	int mainMenuItemHeight = 40;
	int mainMenuIconLeft = 21;
	int mainMenuTextLeft = 61;
	int mainMenuSkip = 6;
	int mainMenuFooterLeft = 25;
	int mainMenuFooterHeightMin = 80;
	int boxRadius = 8;
	int boxWidth = 320;
	int boxWideWidth = 364;
	int boxTitleHeight = 48;
	int boxButtonHeight = 34;
	static constexpr const char *appVersion = "0.14";

	QFont fontName;
	QFont fontText;
	QFont fontPreview;
	QFont fontDate;
	QFont fontBadge;
	QFont fontHeader;
	QFont fontStatus;
	QFont fontService;
	QFont fontUserpic;
	QFont fontBoxTitle;
	QFont fontBoxButton;
	QFont fontMono;
	QFont fontMenuItem;

	static const Theme &current();
	static void setCurrent(const Theme &theme);
	static const Theme &light();
	static const Theme &night();
	static void loadFonts();
	static ThemeNotifier &notifier();
	static void applyApplicationPalette();
	static const class QPixmap &wallpaper();
	static int userpicIndex(const QString &id);
	static QString initialLetter(const QString &name);
};
