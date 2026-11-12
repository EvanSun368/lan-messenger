#pragma once

#include "app_settings.h"
#include <QMainWindow>

class QMouseEvent;

class ChatView;
class DialogList;
class NodeController;
class MainMenuPanel;
class QWidget;
class QPropertyAnimation;

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(NodeController *controller, QWidget *parent = nullptr);
	void selectChat(const QString &id);

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void keyPressEvent(class QKeyEvent *event) override;
	void resizeEvent(class QResizeEvent *event) override;

private:
	void refreshDialogs();
	void refreshChat();
	void openMenu();
	void refreshMenuContents();
	void closeMenu();
	void finishCloseMenu();
	void startNewGroup();
	void chooseFile();
	void chooseFolder();
	void choosePhoto();
	void openSettings();
	void showPeerInfo();
	void layoutChrome();
	void sendDropped(const QStringList &paths);
	void toggleNight();
	void openDownloads();
	void showAbout();
	void editProfile();
	void editMembers();
	void applyTheme();

	NodeController *controller_ = nullptr;
	DialogList *dialogs_ = nullptr;
	ChatView *chat_ = nullptr;
	QWidget *menuLayer_ = nullptr;
	MainMenuPanel *menu_ = nullptr;
	QPropertyAnimation *menuAnim_ = nullptr;
	QString activeChat_;
	AppSettings settings_;
	class CaptionButtons *caption_ = nullptr;
};
