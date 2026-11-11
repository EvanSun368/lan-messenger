/* Stubs for the tg_owt-backed parts of lib_webrtc.
 * Used when LANLINK_DISABLE_CALLS is on (MinGW LAN text + stickers).
 * Device listing still uses the OpenAL helper compiled into lib_webrtc.
 */
#include "webrtc/webrtc_environment.h"
#include "webrtc/webrtc_video_track.h"
#include "webrtc/webrtc_audio_input_tester.h"
#include "webrtc/webrtc_system_audio_capture.h"
#include "webrtc/platform/webrtc_platform_environment.h"
#include "webrtc/details/webrtc_environment_video_capture.h"

#include <atomic>
#include <memory>

namespace Webrtc::Platform {
namespace {

class DummyEnvironment final : public Environment {
public:
	explicit DummyEnvironment(not_null<EnvironmentDelegate*>) {
	}

	QString defaultId(DeviceType) override {
		return {};
	}
	DeviceInfo device(DeviceType, const QString &id) override {
		return { .id = id };
	}
	std::vector<DeviceInfo> devices(DeviceType) override {
		return {};
	}
	bool refreshFullListOnChange(DeviceType) override {
		return false;
	}
	bool desktopCaptureAllowed() const override {
		return false;
	}
	std::optional<QString> uniqueDesktopCaptureSource() const override {
		return std::nullopt;
	}
	void defaultIdRequested(DeviceType) override {
	}
	void devicesRequested(DeviceType) override {
	}
};

} // namespace

std::unique_ptr<Environment> CreateEnvironment(
	not_null<EnvironmentDelegate*> delegate) {
	return std::make_unique<DummyEnvironment>(delegate);
}

} // namespace Webrtc::Platform

namespace Webrtc::details {

EnvironmentVideoCapture::EnvironmentVideoCapture(
	not_null<EnvironmentDelegate*> delegate)
: _delegate(delegate) {
}

EnvironmentVideoCapture::~EnvironmentVideoCapture() = default;

QString EnvironmentVideoCapture::defaultId(DeviceType) {
	return {};
}

DeviceInfo EnvironmentVideoCapture::device(DeviceType, const QString &id) {
	return { .id = id };
}

std::vector<DeviceInfo> EnvironmentVideoCapture::devices(DeviceType) {
	return {};
}

bool EnvironmentVideoCapture::refreshFullListOnChange(DeviceType) {
	return false;
}

bool EnvironmentVideoCapture::desktopCaptureAllowed() const {
	return false;
}

std::optional<QString> EnvironmentVideoCapture::uniqueDesktopCaptureSource() const {
	return std::nullopt;
}

void EnvironmentVideoCapture::defaultIdRequested(DeviceType) {
}

void EnvironmentVideoCapture::devicesRequested(DeviceType) {
}

QString EnvironmentVideoCapture::DefaultId() {
	return {};
}

} // namespace Webrtc::details

namespace rtc {
template <typename T>
class VideoSinkInterface {
public:
	virtual ~VideoSinkInterface() = default;
};
} // namespace rtc

namespace webrtc {
class VideoFrame {};
} // namespace webrtc

namespace Webrtc {

class VideoTrack::Sink final : public SinkInterface {
public:
};

struct VideoTrack::Frame {};

VideoTrack::VideoTrack(VideoState state, bool)
: _state(state) {
}

VideoTrack::~VideoTrack() = default;

void VideoTrack::markFrameShown() {
}

QImage VideoTrack::frame(const FrameRequest &) {
	return {};
}

FrameWithInfo VideoTrack::frameWithInfo(bool) const {
	return {};
}

QSize VideoTrack::frameSize() const {
	return {};
}

rpl::producer<> VideoTrack::renderNextFrame() const {
	return rpl::never<>();
}

std::shared_ptr<SinkInterface> VideoTrack::sink() {
	return nullptr;
}

VideoState VideoTrack::state() const {
	return _state.current();
}

rpl::producer<VideoState> VideoTrack::stateValue() const {
	return _state.value();
}

rpl::producer<VideoState> VideoTrack::stateChanges() const {
	return _state.changes();
}

void VideoTrack::setState(VideoState state) {
	_state = state;
}

void VideoTrack::PrepareFrameByRequests(not_null<Frame*>, int) {
}

class AudioInputTester::Impl {
};

AudioInputTester::AudioInputTester(rpl::producer<DeviceResolvedId>)
: _maxSample(std::make_shared<std::atomic<int>>(0)) {
}

AudioInputTester::~AudioInputTester() = default;

float AudioInputTester::getAndResetLevel() {
	return 0.f;
}

/* SystemAudioCaptureSupported / CreateSystemAudioCapture live in
 * webrtc_system_audio_capture.cpp — do not redefine them here. */

} // namespace Webrtc
