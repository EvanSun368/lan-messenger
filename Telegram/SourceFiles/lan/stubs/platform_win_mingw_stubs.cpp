#include <windows.h>

/* MinGW-w64 headers declare this Win10 GUID but do not define it. */
extern "C" const GUID GUID_POWER_SAVING_STATUS = {
	0xE00958C0, 0xC213, 0x4ACE,
	{ 0xAC, 0x77, 0xFE, 0xCC, 0xED, 0x2E, 0xE7, 0x1E }
};

#include "platform/win/notifications_manager_win.h"
#include "platform/win/windows_autostart_task.h"
#include "platform/win/wallet_protection_win.h"
#include "window/notifications_manager.h"

namespace Platform::Notifications {

class Manager::Private {};

Manager::Manager(Window::Notifications::System *system)
: NativeManager(system)
, _private(std::make_unique<Private>()) {
}

Manager::~Manager() = default;

bool Manager::init() {
	return false;
}

void Manager::clearNotification(NotificationId) {
}

void Manager::handleActivation(const ToastActivation &) {
}

void Manager::doShowNativeNotification(
		NotificationInfo &&,
		Ui::PeerUserpicView &) {
}

void Manager::doClearAllFast() {
}

void Manager::doClearFromItem(not_null<HistoryItem*>) {
}

void Manager::doClearFromTopic(not_null<Data::ForumTopic*>) {
}

void Manager::doClearFromSublist(not_null<Data::SavedSublist*>) {
}

void Manager::doClearFromHistory(not_null<History*>) {
}

void Manager::doClearFromSession(not_null<Main::Session*>) {
}

void Manager::onBeforeNotificationActivated(NotificationId) {
}

void Manager::onAfterNotificationActivated(
		NotificationId,
		not_null<Window::SessionController*>) {
}

bool Manager::doSkipToast() const {
	return false;
}

void Manager::doMaybePlaySound(Fn<void()> playSound) {
	if (playSound) {
		playSound();
	}
}

void Manager::doMaybeFlashBounce(Fn<void()> flashBounce) {
	if (flashBounce) {
		flashBounce();
	}
}

bool SkipToastForCustom() {
	return false;
}

void MaybePlaySoundForCustom(Fn<void()> playSound) {
	if (playSound) {
		playSound();
	}
}

void MaybeFlashBounceForCustom(Fn<void()> flashBounce) {
	if (flashBounce) {
		flashBounce();
	}
}

bool WaitForInputForCustom() {
	return true;
}

bool Supported() {
	return false;
}

bool Enforced() {
	return false;
}

bool ByDefault() {
	return false;
}

bool VolumeSupported() {
	return false;
}

void Create(Window::Notifications::System *system) {
	system->setManager([=] { return nullptr; });
}

} // namespace Platform::Notifications

namespace Platform::AutostartTask {

void Toggle(bool, Fn<void(bool)> done) {
	if (done) {
		done(false);
	}
}

void RequestState(Fn<void(bool)> callback) {
	if (callback) {
		callback(false);
	}
}

void OpenSettings() {
}

} // namespace Platform::AutostartTask

namespace Platform {

void RegisterWalletProtectionProvider() {
}

} // namespace Platform
