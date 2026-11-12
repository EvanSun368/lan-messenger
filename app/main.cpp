#include "main_window.h"
#include "node_controller.h"
#include "theme.h"
#include "i18n.h"
#include "app_settings.h"
#include "working_dir.h"

#include <QApplication>
#include <QTimer>
#include <QScreen>
#include <QIcon>
#include <filesystem>
#include <iostream>
#include <string>
#include <cstdio>

namespace {

std::string argument_value(int argc, char **argv, const std::string &key) {
	const auto prefix = key + "=";
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i] ? argv[i] : "";
		if (arg == key && i + 1 < argc) {
			return argv[i + 1];
		}
		if (arg.rfind(prefix, 0) == 0) {
			return arg.substr(prefix.size());
		}
	}
	return {};
}

} // namespace

int main(int argc, char *argv[]) {
	// Parse before QApplication: Qt's X11 layer consumes --name.
	const auto dir = resolve_working_dir(argc, argv);
	const auto name = argument_value(argc, argv, "--name");

	QApplication app(argc, argv);
	app.setApplicationName(QStringLiteral("LanLink"));
	app.setOrganizationName(QStringLiteral("LanLink"));
	QIcon icon;
	icon.addFile(QStringLiteral(":/art/lanlink.ico"));
	icon.addFile(QStringLiteral(":/art/lanlink.png"));
	icon.addFile(QStringLiteral(":/art/lanlink-64.png"));
	app.setWindowIcon(icon);

	i18n::initFromLocale();
	auto settings = AppSettings::load(dir);
	const auto langArg = argument_value(argc, argv, "--lang");
	if (langArg == "zh" || langArg == "zh-Hans" || langArg == "zh-CN" || langArg == "zh_CN") {
		settings.language = Language::ChineseSimplified;
	} else if (langArg == "zh-Hant" || langArg == "zh-TW" || langArg == "zh_TW") {
		settings.language = Language::ChineseTraditional;
	} else if (langArg == "en") {
		settings.language = Language::English;
	}
	i18n::setLanguage(settings.language);
	if (!langArg.empty()) {
		settings.save(dir);
	}

	Theme::loadFonts();
	Theme::setCurrent(settings.dark ? Theme::night() : Theme::light());
	Theme::applyApplicationPalette();

	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	std::cout << "LanLink working dir: " << dir.string() << std::endl;
	if (!name.empty()) {
		std::cout << "LanLink display name: " << name << std::endl;
	}

	NodeController controller(dir, name);
	const auto peer = argument_value(argc, argv, "--peer");
	if (!peer.empty()) {
		const auto colon = peer.rfind(':');
		if (colon != std::string::npos) {
			const auto host = peer.substr(0, colon);
			const auto port = static_cast<quint16>(std::stoi(peer.substr(colon + 1)));
			controller.addDiscoveryTarget(QString::fromStdString(host), port);
		}
	}

	std::cout << "LanLink udp port: " << controller.udpPort() << std::endl;
	std::cout.flush();

	MainWindow window(&controller);
	window.show();
	const auto geometry = argument_value(argc, argv, "--geometry");
	if (!geometry.empty()) {
		int x = 40, y = 40, w = 1100, h = 720;
		std::sscanf(geometry.c_str(), "%dx%d+%d+%d", &w, &h, &x, &y);
		window.setGeometry(x, y, w, h);
	}
	if (argument_value(argc, argv, "--demo") == "1"
		|| argument_value(argc, argv, "--demo") == "true") {
		auto *demoTimer = new QTimer(&window);
		demoTimer->setInterval(300);
		QObject::connect(demoTimer, &QTimer::timeout, &window, [demoTimer, &controller, &window]() {
			const auto peers = controller.onlinePeers();
			if (peers.isEmpty()) {
				return;
			}
			demoTimer->stop();
			const auto chat = peers.first().id;
			window.selectChat(chat);
			const auto animals = QString::fromUcs4(
				U"\U0001F436\U0001F408\U0001F43B\U0001F43A\U0001F47B\U0001F60E\U0001F603");
			controller.sendText(chat, animals);
			const auto tools = QString::fromUcs4(
				U"\U0001F60A \u2702\uFE0F \U0001F48E \U0001F3A8 \U0001F527 \U0001F4A1 \U0001F4AA \U0001F44D");
			controller.sendText(chat, tools);
			controller.sendText(
				chat,
				QString::fromUcs4(U"Hello \u4F60\u597D, LanLink! \U0001F60A"));
			controller.sendText(chat, QStringLiteral("Recall this"));
			QTimer::singleShot(400, &controller, [&controller, chat]() {
				const auto messages = controller.messages(chat);
				for (int i = messages.size() - 1; i >= 0; --i) {
					if (messages[i].outgoing && messages[i].text == QStringLiteral("Recall this")) {
						controller.recallMessage(chat, messages[i].id);
						break;
					}
				}
			});
		});
		demoTimer->start();
	}
	return app.exec();

}
