/* Stub header used when LANLINK_DISABLE_CALLS is on and tg_owt is not built.
 * Matches the official tgcalls/VideoCaptureInterface.h surface enough for
 * video-note UI to compile; methods are no-ops and do not capture a camera.
 */
#ifndef TGCALLS_VIDEO_CAPTURE_INTERFACE_H
#define TGCALLS_VIDEO_CAPTURE_INTERFACE_H

#include <functional>
#include <memory>
#include <string>

namespace rtc {
template <typename VideoFrameT>
class VideoSinkInterface;
} // namespace rtc

namespace webrtc {
class VideoFrame;
} // namespace webrtc

namespace tgcalls {

class PlatformContext;
class Threads;

enum class VideoState {
	Inactive,
	Paused,
	Active,
};

class VideoCaptureInterface {
public:
	static std::unique_ptr<VideoCaptureInterface> Create(
		std::shared_ptr<Threads> = nullptr,
		std::string = {},
		bool = false,
		std::shared_ptr<PlatformContext> = nullptr) {
		return std::make_unique<VideoCaptureInterface>();
	}

	VideoCaptureInterface() = default;
	virtual ~VideoCaptureInterface() = default;

	virtual void switchToDevice(std::string, bool) {
	}
	virtual void setState(VideoState) {
	}
	virtual void setPreferredAspectRatio(float) {
	}
	virtual void setOutput(
		std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>>) {
	}
	virtual void setOnFatalError(std::function<void()>) {
	}
	virtual void setOnPause(std::function<void(bool)>) {
	}
	virtual void setOnIsActiveUpdated(std::function<void(bool)>) {
	}
	virtual void withNativeImplementation(std::function<void(void *)> completion) {
		if (completion) {
			completion(nullptr);
		}
	}
};

} // namespace tgcalls

#endif
