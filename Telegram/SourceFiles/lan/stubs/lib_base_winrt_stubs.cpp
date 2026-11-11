/* MinGW stand-ins for C++/WinRT-backed lib_base Windows features. */
#include "base/platform/base_platform_system_media_controls.h"
#include "base/system_unlock.h"
#include "base/platform/win/base_windows_winrt.h"

#include <memory>

namespace base::Platform {

struct SystemMediaControls::Private {};

SystemMediaControls::SystemMediaControls()
: _private(std::make_unique<Private>()) {
}

SystemMediaControls::~SystemMediaControls() = default;

bool SystemMediaControls::init() {
	return false;
}

bool SystemMediaControls::seekingSupported() const {
	return false;
}

bool SystemMediaControls::volumeSupported() const {
	return false;
}

void SystemMediaControls::setApplicationName(const QString &) {
}

void SystemMediaControls::setEnabled(bool) {
}

void SystemMediaControls::setIsNextEnabled(bool) {
}

void SystemMediaControls::setIsPreviousEnabled(bool) {
}

void SystemMediaControls::setIsPlayPauseEnabled(bool) {
}

void SystemMediaControls::setIsStopEnabled(bool) {
}

void SystemMediaControls::setPlaybackStatus(PlaybackStatus) {
}

void SystemMediaControls::setLoopStatus(LoopStatus) {
}

void SystemMediaControls::setShuffle(bool) {
}

void SystemMediaControls::setTitle(const QString &) {
}

void SystemMediaControls::setArtist(const QString &) {
}

void SystemMediaControls::setThumbnail(const QImage &) {
}

void SystemMediaControls::setDuration(int) {
}

void SystemMediaControls::setPosition(int) {
}

void SystemMediaControls::setPlaybackRate(float64) {
}

void SystemMediaControls::setVolume(float64) {
}

void SystemMediaControls::clearThumbnail() {
}

void SystemMediaControls::clearMetadata() {
}

void SystemMediaControls::updateDisplay() {
}

rpl::producer<SystemMediaControls::Command> SystemMediaControls::commandRequests() const {
	return rpl::never<Command>();
}

rpl::producer<float64> SystemMediaControls::seekRequests() const {
	return rpl::never<float64>();
}

rpl::producer<float64> SystemMediaControls::volumeChangeRequests() const {
	return rpl::never<float64>();
}

rpl::producer<> SystemMediaControls::updatePositionRequests() const {
	return rpl::never<>();
}

bool SystemMediaControls::Supported() {
	return false;
}

} // namespace base::Platform

namespace base {

rpl::producer<SystemUnlockAvailability> SystemUnlockStatus(bool) {
	return rpl::single(SystemUnlockAvailability{ .known = true });
}

void SuggestSystemUnlock(
	not_null<QWidget*>,
	const QString &,
	Fn<void(SystemUnlockResult)> done) {
	if (done) {
		done(SystemUnlockResult::Cancelled);
	}
}

} // namespace base

namespace base::WinRT {

bool Supported() {
	return false;
}

namespace details {

void LogError(HRESULT, const QString &, const std::source_location &) {
}

} // namespace details

} // namespace base::WinRT
