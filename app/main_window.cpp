#include "main_window.h"

#include "avatar.h"
#include "chat_view.h"
#include "dialog_list.h"
#include "members_box.h"
#include "new_group_box.h"
#include "profile_box.h"
#include "settings_box.h"
#include "node_controller.h"
#include "theme.h"
#include "i18n.h"
#include "main_menu.h"
#include "official_box.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>

#include <functional>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QLabel>


class CaptionButtons : public QWidget {
public:
	explicit CaptionButtons(QWidget *parent = nullptr)
		: QWidget(parent) {
		setFixedSize(108, 40);
		setMouseTracking(true);
	}

	QRect minRect() const { return QRect(0, 0, 36, 40); }
	QRect maxRect() const { return QRect(36, 0, 36, 40); }
	QRect closeRect() const { return QRect(72, 0, 36, 40); }

protected:
	void paintEvent(QPaintEvent *) override {
		const auto &theme = Theme::current();
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		auto drawHover = [&](const QRect &rect, bool hover, const QColor &color) {
			if (hover) {
				painter.setPen(Qt::NoPen);
				painter.setBrush(color);
				painter.drawRect(rect);
			}
		};
		drawHover(minRect(), hover_ == 0, theme.windowBgOver);
		drawHover(maxRect(), hover_ == 1, theme.windowBgOver);
		drawHover(closeRect(), hover_ == 2, QColor("#e81123"));
		const auto fg = theme.windowFg;
		const auto closeFg = hover_ == 2 ? QColor("#ffffff") : fg;
		QPen pen(fg, 1.4);
		pen.setCapStyle(Qt::RoundCap);
		painter.setPen(pen);
		const auto minC = minRect().center();
		painter.drawLine(minC.x() - 5, minC.y(), minC.x() + 5, minC.y());
		const auto maxC = maxRect().center();
		painter.drawRect(QRect(maxC.x() - 5, maxC.y() - 5, 10, 10));
		pen.setColor(closeFg);
		painter.setPen(pen);
		const auto closeC = closeRect().center();
		painter.drawLine(closeC.x() - 5, closeC.y() - 5, closeC.x() + 5, closeC.y() + 5);
		painter.drawLine(closeC.x() + 5, closeC.y() - 5, closeC.x() - 5, closeC.y() + 5);
	}
	void mouseMoveEvent(QMouseEvent *event) override {
		int hover = -1;
		if (minRect().contains(event->pos())) hover = 0;
		else if (maxRect().contains(event->pos())) hover = 1;
		else if (closeRect().contains(event->pos())) hover = 2;
		if (hover != hover_) { hover_ = hover; update(); }
	}
	void leaveEvent(QEvent *) override { hover_ = -1; update(); }
	void mousePressEvent(QMouseEvent *event) override {
		auto *window = this->window();
		if (!window) return;
		if (minRect().contains(event->pos())) {
			window->showMinimized();
		} else if (maxRect().contains(event->pos())) {
			if (window->isMaximized()) window->showNormal();
			else window->showMaximized();
		} else if (closeRect().contains(event->pos())) {
			window->close();
		}
	}
private:
	int hover_ = -1;
};

// Official tdesktop shows the hamburger menu as Window::MainMenu inside
// Ui::LayerStackWidget (lib_ui/ui/layers/layer_widget.cpp). It does not
// auto-hide when the pointer leaves the panel. It closes when the user
// clicks the dimmed background (backgroundClicked, _hideByBackgroundClick
// defaults to true) or presses Escape (LayerStackWidget::keyPressEvent).
class MenuLayer : public QWidget {
public:
	explicit MenuLayer(QWidget *parent = nullptr)
		: QWidget(parent) {
		hide();
		setFocusPolicy(Qt::StrongFocus);
		// Official BackgroundWidget drops WA_OpaquePaintEvent after the
		// show animation so the chat stays visible under st::layerBg.
		setAttribute(Qt::WA_NoSystemBackground);
		setAutoFillBackground(false);
	}

	void setPanel(QWidget *panel) {
		panel_ = panel;
	}

	void setCloseCallback(std::function<void()> close) {
		close_ = std::move(close);
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter painter(this);
		const int left = panel_
			? qMax(0, panel_->x() + panel_->width())
			: 0;
		// Official BackgroundWidget fills from mainMenuRight to the
		// window edge with st::layerBg (#0000007f).
		painter.fillRect(QRect(left, 0, width() - left, height()), Theme::current().layerBg);
		if (left > 0) {
			// Official paints st::boxRoundShadow on the menu's right edge
			// when the show/hide cache is not in use.
			QLinearGradient shadow(left, 0, left + 12, 0);
			shadow.setColorAt(0, QColor(0, 0, 0, 60));
			shadow.setColorAt(1, QColor(0, 0, 0, 0));
			painter.fillRect(QRect(left, 0, 12, height()), shadow);
		}
	}

	void mousePressEvent(QMouseEvent *event) override {
		if (!panel_ || event->pos().x() >= panel_->x() + panel_->width()
			|| event->pos().y() < panel_->y()
			|| event->pos().y() >= panel_->y() + panel_->height()) {
			if (close_) {
				close_();
			}
		}
	}

	void keyPressEvent(QKeyEvent *event) override {
		if (event->key() == Qt::Key_Escape) {
			if (close_) {
				close_();
			}
			return;
		}
		QWidget::keyPressEvent(event);
	}

private:
	QWidget *panel_ = nullptr;
	std::function<void()> close_;
};

MainWindow::MainWindow(NodeController *controller, QWidget *parent)
	: QMainWindow(parent)
	, controller_(controller) {
	settings_ = AppSettings::load(controller_->workingDir());
	controller_->setEncryptFiles(settings_.encryptFiles);
	if (!settings_.saveFolder.isEmpty()) {
		controller_->setDownloadsDir(std::filesystem::path(settings_.saveFolder.toStdString()));
	}
	setWindowTitle(QStringLiteral("LanLink"));
	setWindowFlags(windowFlags() | Qt::Window | Qt::WindowMinimizeButtonHint
		| Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
	resize(1100, 720);
	setMinimumSize(800, 520);

	auto *root = new QWidget(this);
	auto *layout = new QHBoxLayout(root);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	dialogs_ = new DialogList(root);
	chat_ = new ChatView(root);
	layout->addWidget(dialogs_);
	layout->addWidget(chat_, 1);
	setCentralWidget(root);
	// Native window decorations already provide min/max/close. Do not
	// paint a second caption cluster over the chat header (Windows
	// showed both, which the user flagged as 重复了).

	menuLayer_ = new MenuLayer(root);
	menu_ = new MainMenuPanel(menuLayer_);
	menu_->setFixedWidth(Theme::current().mainMenuWidth);
	menu_->hide();
	auto *layer = static_cast<MenuLayer *>(menuLayer_);
	layer->setPanel(menu_);
	layer->setCloseCallback([this] { closeMenu(); });
	menuAnim_ = new QPropertyAnimation(menu_, "pos", this);
	menuAnim_->setDuration(200);
	menuAnim_->setEasingCurve(QEasingCurve::InOutSine);
	connect(menuAnim_, &QPropertyAnimation::valueChanged, this, [this]() {
		if (menuLayer_) {
			menuLayer_->update();
		}
	});
	connect(menu_, &MainMenuPanel::aboutClicked, this, &MainWindow::showAbout);

	connect(dialogs_, &DialogList::chatSelected, this, [this](const QString &id) {
		closeMenu();
		activeChat_ = id;
		controller_->setActiveChat(id);
		refreshChat();
		chat_->focusCompose();
	});
	connect(dialogs_, &DialogList::menuRequested, this, &MainWindow::openMenu);
	connect(chat_, &ChatView::sendText, this, [this](const QString &text) {
		if (controller_->sendText(activeChat_, text)) {
			chat_->clearCompose();
		}
	});
	connect(chat_, &ChatView::attachFile, this, &MainWindow::chooseFile);
	connect(chat_, &ChatView::attachFolder, this, &MainWindow::chooseFolder);
	connect(chat_, &ChatView::attachPhoto, this, &MainWindow::choosePhoto);
	connect(chat_, &ChatView::showPeerInfo, this, &MainWindow::showPeerInfo);
	connect(chat_, &ChatView::sendPaths, this, &MainWindow::sendDropped);
	connect(chat_, &ChatView::cancelTransfer, this, [this](const QString &id) {
		controller_->abortTransfer(activeChat_, id);
	});
	connect(chat_, &ChatView::recallMessage, this, [this](const QString &id) {
		controller_->recallMessage(activeChat_, id);
	});
	connect(chat_, &ChatView::editMembers, this, &MainWindow::editMembers);
	connect(chat_, &ChatView::acceptTransfer, this, [this](const QString &id) {
		controller_->acceptTransfer(id);
	});
	connect(chat_, &ChatView::rejectTransfer, this, [this](const QString &id) {
		controller_->rejectTransfer(id);
	});
	connect(chat_, &ChatView::chooseSavePath, this, [this](const QString &id, const QString &name, bool isFolder) {
		if (isFolder) {
			const auto start = QString::fromStdString(controller_->downloadsDir().u8string());
			const auto path = QFileDialog::getExistingDirectory(this, i18n::tr("choose_save_location"), start);
			if (!path.isEmpty()) {
				controller_->acceptTransfer(id, path);
			}
			return;
		}
		const auto startDir = QString::fromStdString(controller_->downloadsDir().u8string());
		const auto path = QFileDialog::getSaveFileName(
			this,
			i18n::tr("choose_save_location"),
			startDir + QChar('/') + name);
		if (!path.isEmpty()) {
			controller_->acceptTransfer(id, path);
		}
	});
	connect(&Theme::notifier(), &ThemeNotifier::changed, this, &MainWindow::applyTheme);
	connect(controller_, &NodeController::dialogsChanged, this, &MainWindow::refreshDialogs);
	connect(controller_, &NodeController::chatChanged, this, [this](const QString &id) {
		if (id == activeChat_) {
			refreshChat();
		}
		refreshDialogs();
	});
	connect(controller_, &NodeController::selfReady, this, [this](const QString &name) {
		Q_UNUSED(name);
		dialogs_->setSelf(controller_->selfId(), controller_->selfName());
		chat_->setSelf(controller_->selfId(), controller_->selfName());
		refreshDialogs();
	});
	connect(&i18n::notifier(), &LanguageNotifier::changed, this, [this]() {
		if (menuLayer_ && menuLayer_->isVisible()) {
			refreshMenuContents();
		}
		refreshDialogs();
		refreshChat();
		update();
	});
	layoutChrome();
}

void MainWindow::layoutChrome() {
	if (caption_) {
		caption_->move(width() - caption_->width() - 4, 8);
		caption_->raise();
	}
	if (menuLayer_) {
		menuLayer_->setGeometry(centralWidget() ? centralWidget()->rect() : QRect(0, 0, width(), height()));
		menuLayer_->raise();
		if (menu_ && menu_->isVisible()) {
			const int w = Theme::current().mainMenuWidth;
			const int x = (menuAnim_ && menuAnim_->state() == QAbstractAnimation::Running)
				? menu_->x()
				: 0;
			menu_->setGeometry(x, 0, w, menuLayer_->height());
			menu_->raise();
		}
	}
}

void MainWindow::refreshDialogs() {
	dialogs_->setDialogs(controller_->dialogs());
	dialogs_->setSelected(activeChat_);
}

void MainWindow::refreshChat() {
	if (activeChat_.isEmpty()) {
		chat_->clearChat();
		return;
	}
	chat_->setDialog(controller_->dialog(activeChat_), controller_->messages(activeChat_));
}

void MainWindow::selectChat(const QString &id) {
	if (id.isEmpty()) {
		return;
	}
	activeChat_ = id;
	controller_->setActiveChat(id);
	refreshDialogs();
	refreshChat();
	chat_->focusCompose();
}

void MainWindow::refreshMenuContents() {
	if (!menu_) {
		return;
	}
	const auto &theme = Theme::current();
	menu_->setFixedWidth(theme.mainMenuWidth);
	menu_->setIdentity(controller_->selfId(), controller_->selfName(), i18n::tr("lanlink_local"));
	menu_->setVersionText(
		QStringLiteral("LanLink"),
		i18n::tr("version").arg(QString::fromUtf8(Theme::appVersion))
			+ QStringLiteral("  –  ") + i18n::tr("about"));

	QVector<MainMenuPanel::Item> items;
	auto add = [&](const QString &id, const QString &text, const QString &icon, auto slot, bool sep = false) {
		MainMenuPanel::Item item;
		item.id = id;
		item.text = text;
		item.icon = icon;
		item.separatorBefore = sep;
		item.onClick = [this, slot]() { (this->*slot)(); };
		items.push_back(item);
	};
	add(QStringLiteral("profile"), i18n::tr("edit_profile"),
		QStringLiteral(":/icons/menu/profile.png"), &MainWindow::editProfile);
	add(QStringLiteral("group"), i18n::tr("new_group"),
		QStringLiteral(":/icons/menu/groups.png"), &MainWindow::startNewGroup, true);
	add(QStringLiteral("downloads"), i18n::tr("downloads"),
		QStringLiteral(":/icons/menu/download.png"), &MainWindow::openDownloads);
	add(QStringLiteral("settings"), i18n::tr("settings"),
		QStringLiteral(":/icons/menu/settings.png"), &MainWindow::openSettings);
	MainMenuPanel::Item night;
	night.id = QStringLiteral("night");
	night.text = i18n::tr("night_mode");
	night.icon = QStringLiteral(":/icons/menu/night_mode.png");
	night.toggle = true;
	night.toggled = theme.dark;
	night.onClick = [this]() { toggleNight(); };
	items.push_back(night);
	menu_->setItems(items);
	menu_->update();
	if (menuLayer_) {
		menuLayer_->update();
	}
}

void MainWindow::openMenu() {
	const int w = Theme::current().mainMenuWidth;
	refreshMenuContents();
	menuLayer_->setGeometry(centralWidget() ? centralWidget()->rect() : QRect(0, 0, width(), height()));
	const bool alreadyOpen = menuLayer_->isVisible()
		&& menu_->isVisible()
		&& menuAnim_->state() != QAbstractAnimation::Running
		&& menu_->pos() == QPoint(0, 0);
	menuLayer_->show();
	menuLayer_->raise();
	menu_->show();
	menu_->raise();
	if (alreadyOpen) {
		menu_->setGeometry(0, 0, w, menuLayer_->height());
		layoutChrome();
		menuLayer_->setFocus(Qt::OtherFocusReason);
		if (caption_) {
			caption_->raise();
		}
		return;
	}
	menu_->setGeometry(-w, 0, w, menuLayer_->height());
	if (menuAnim_->state() == QAbstractAnimation::Running) {
		menuAnim_->stop();
	}
	disconnect(menuAnim_, &QPropertyAnimation::finished, this, &MainWindow::finishCloseMenu);
	menuAnim_->setStartValue(QPoint(-w, 0));
	menuAnim_->setEndValue(QPoint(0, 0));
	menuAnim_->start();
	layoutChrome();
	menuLayer_->setFocus(Qt::OtherFocusReason);
	if (caption_) {
		caption_->raise();
	}
}

void MainWindow::closeMenu() {
	if (!menuLayer_ || !menuLayer_->isVisible()) {
		return;
	}
	const int w = Theme::current().mainMenuWidth;
	if (menuAnim_->state() == QAbstractAnimation::Running) {
		menuAnim_->stop();
	}
	disconnect(menuAnim_, &QPropertyAnimation::finished, this, &MainWindow::finishCloseMenu);
	connect(menuAnim_, &QPropertyAnimation::finished, this, &MainWindow::finishCloseMenu);
	menuAnim_->setStartValue(menu_->pos());
	menuAnim_->setEndValue(QPoint(-w, 0));
	menuAnim_->start();
}

void MainWindow::finishCloseMenu() {
	if (menu_) {
		menu_->hide();
	}
	if (menuLayer_) {
		menuLayer_->hide();
	}
}

void MainWindow::startNewGroup() {
	closeMenu();
	NewGroupBox box(controller_->onlinePeers(), this);
	if (box.exec() != QDialog::Accepted) {
		return;
	}
	const auto id = controller_->createGroup(box.groupName(), box.memberIds());
	if (id.isEmpty()) {
		return;
	}
	activeChat_ = id;
	controller_->setActiveChat(id);
	refreshDialogs();
	refreshChat();
}

void MainWindow::chooseFile() {
	const auto path = QFileDialog::getOpenFileName(this, i18n::tr("send_file"));
	if (!path.isEmpty()) {
		controller_->sendPath(activeChat_, path);
	}
}

void MainWindow::chooseFolder() {
	const auto path = QFileDialog::getExistingDirectory(this, i18n::tr("send_folder"));
	if (!path.isEmpty()) {
		controller_->sendPath(activeChat_, path);
	}
}

void MainWindow::sendDropped(const QStringList &paths) {
	if (activeChat_.isEmpty()) {
		return;
	}
	for (const auto &path : paths) {
		controller_->sendPath(activeChat_, path);
	}
}

void MainWindow::applyTheme() {
	const auto &theme = Theme::current();
	auto pal = palette();
	pal.setColor(QPalette::Window, theme.windowBg);
	pal.setColor(QPalette::WindowText, theme.windowFg);
	setPalette(pal);
	dialogs_->applyTheme();
	chat_->applyTheme();
	if (caption_) {
		caption_->update();
		layoutChrome();
	}
	// Official keeps Window::MainMenu and its dim layer in place and only
	// retints colors. Never close, hide, or restart the 200 ms slide.
	if (menuLayer_ && menuLayer_->isVisible()) {
		refreshMenuContents();
	}
	update();
}

void MainWindow::toggleNight() {
	Theme::setCurrent(Theme::current().dark ? Theme::light() : Theme::night());
	settings_.dark = Theme::current().dark;
	settings_.save(controller_->workingDir());
}

void MainWindow::editProfile() {
	closeMenu();
	ProfileBox box(
		controller_->selfId(),
		controller_->selfFirstName(),
		controller_->selfLastName(),
		controller_->selfBio(),
		controller_->selfAvatarPath(),
		QString(),
		true,
		this);
	if (box.exec() != QDialog::Accepted) {
		return;
	}
	if (!box.displayName().isEmpty()) {
		controller_->setProfile(box.firstName(), box.lastName(), box.bio());
	}
	if (!box.avatarPath().isEmpty() && box.avatarPath() != controller_->selfAvatarPath()) {
		controller_->setAvatarFromPath(box.avatarPath());
	}
	dialogs_->setSelf(controller_->selfId(), controller_->selfName());
	chat_->setSelf(controller_->selfId(), controller_->selfName());
	refreshDialogs();
	refreshChat();
}

void MainWindow::showPeerInfo() {
	if (activeChat_.isEmpty()) {
		return;
	}
	const auto dialog = controller_->dialog(activeChat_);
	if (dialog.isGroup) {
		editMembers();
		return;
	}
	ProfileBox box(
		dialog.id,
		dialog.firstName.isEmpty() ? dialog.name : dialog.firstName,
		dialog.lastName,
		dialog.bio,
		QString(),
		dialog.ip,
		false,
		this);
	box.exec();
}

void MainWindow::choosePhoto() {
	const auto path = QFileDialog::getOpenFileName(
		this,
		i18n::tr("send_photo"),
		QString(),
		i18n::tr("images_filter"));
	if (!path.isEmpty()) {
		controller_->sendPath(activeChat_, path);
	}
}

void MainWindow::openSettings() {
	closeMenu();
	settings_.dark = Theme::current().dark;
	settings_.encryptFiles = controller_->encryptFiles();
	SettingsBox box(settings_, this);
	if (box.exec() != QDialog::Accepted) {
		return;
	}
	settings_ = box.settings();
	settings_.save(controller_->workingDir());
	controller_->setEncryptFiles(settings_.encryptFiles);
	if (!settings_.saveFolder.isEmpty()) {
		controller_->setDownloadsDir(std::filesystem::path(settings_.saveFolder.toStdString()));
	}
	i18n::setLanguage(settings_.language);
}

void MainWindow::editMembers() {
	if (activeChat_.isEmpty()) {
		return;
	}
	const auto dialog = controller_->dialog(activeChat_);
	if (!dialog.isGroup) {
		return;
	}
	MembersBox box(dialog, controller_->selfId(), controller_->onlinePeers(), this);
	if (box.exec() != QDialog::Accepted) {
		return;
	}
	const auto next = box.memberIds();
	QStringList current = dialog.memberIds;
	current.removeAll(controller_->selfId());
	for (const auto &id : next) {
		if (!current.contains(id)) {
			controller_->addGroupMember(activeChat_, id);
		}
	}
	for (const auto &id : current) {
		if (!next.contains(id)) {
			controller_->removeGroupMember(activeChat_, id);
		}
	}
	refreshDialogs();
	refreshChat();
}

void MainWindow::openDownloads() {
	closeMenu();
	QDesktopServices::openUrl(
		QUrl::fromLocalFile(QString::fromStdString(controller_->downloadsDir().u8string())));
}

void MainWindow::showAbout() {
	closeMenu();
	OfficialBox box(this);
	box.setBoxTitle(i18n::tr("about_title"));
	box.setBoxWidth(Theme::current().boxWideWidth);
	auto *body = new QLabel(QStringLiteral("<b>LanLink</b><br><br>") + i18n::tr("about_body").toHtmlEscaped().replace(QChar(QChar::LineFeed), QStringLiteral("<br>")), box.body());
	body->setWordWrap(true);
	body->setTextFormat(Qt::RichText);
	body->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::current().windowFg.name()));
	box.bodyLayout()->addWidget(body);
	box.addCloseButton(i18n::tr("ok"));
	box.exec();
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
	if (menuLayer_ && menuLayer_->isVisible() && !menu_->geometry().contains(event->pos())) {
		closeMenu();
	}
	QMainWindow::mousePressEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
	if (event->key() == Qt::Key_Escape && menuLayer_ && menuLayer_->isVisible()) {
		closeMenu();
		return;
	}
	QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
	QMainWindow::resizeEvent(event);
	layoutChrome();
}
