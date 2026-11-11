/* No-op Media::View::VideoStream when official calls are disabled.
 * Intentionally does not include media_view_video_stream.h so this
 * translation unit does not have to destroy unique_ptr<GroupCall>.
 */
#include "data/data_message_reactions.h"
#include "ui/gl/gl_surface.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"

#include <QtGui/QOpenGLFunctions>

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0) && !defined DESKTOP_APP_DISABLE_PRIVATE_QT_API
#include <rhi/qrhi.h>
#endif

namespace ChatHelpers {
class Show;
} // namespace ChatHelpers

namespace Data {
class GroupCall;
} // namespace Data

namespace Calls {
class GroupCall;
} // namespace Calls

namespace Media::View {

class VideoStream final {
public:
	VideoStream(
		not_null<QWidget*>,
		not_null<Ui::RpWidgetWrap*>,
		bool,
		Ui::GL::Backend,
		std::shared_ptr<ChatHelpers::Show>,
		std::shared_ptr<Data::GroupCall>,
		QString,
		MsgId);
	~VideoStream();

	[[nodiscard]] not_null<Calls::GroupCall*> call() const;
	[[nodiscard]] rpl::producer<> closeRequests() const;

	void setVolume(float64);
	void updateGeometry(int, int, int, int);
	void toggleCommentsOn(rpl::producer<bool>);

	void ensureBorrowedRenderer(QOpenGLFunctions &);
	void borrowedPaint(QOpenGLFunctions &);

	void ensureBorrowedRenderer();
	void borrowedPaint(Painter &, const QRegion &);

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0) && !defined DESKTOP_APP_DISABLE_PRIVATE_QT_API
	void borrowedPaintOffscreen(QRhi *, QRhiRenderTarget *, QRhiCommandBuffer *);
	void borrowedPaintOnscreen(QRhi *, QRhiRenderTarget *, QRhiCommandBuffer *);
#endif

	[[nodiscard]] rpl::lifetime &lifetime();

private:
	rpl::event_stream<> _closeRequests;
	rpl::lifetime _lifetime;
};

rpl::producer<std::vector<Data::MessageReactionsTopPaid>> TopVideoStreamDonors(
		not_null<Calls::GroupCall*>) {
	return rpl::single(std::vector<Data::MessageReactionsTopPaid>());
}

VideoStream::VideoStream(
	not_null<QWidget*>,
	not_null<Ui::RpWidgetWrap*>,
	bool,
	Ui::GL::Backend,
	std::shared_ptr<ChatHelpers::Show>,
	std::shared_ptr<Data::GroupCall>,
	QString,
	MsgId) {
}

VideoStream::~VideoStream() = default;

not_null<Calls::GroupCall*> VideoStream::call() const {
	Unexpected("Calls disabled: VideoStream::call");
}

rpl::producer<> VideoStream::closeRequests() const {
	return _closeRequests.events();
}

void VideoStream::setVolume(float64) {
}

void VideoStream::updateGeometry(int, int, int, int) {
}

void VideoStream::toggleCommentsOn(rpl::producer<bool>) {
}

void VideoStream::ensureBorrowedRenderer(QOpenGLFunctions &) {
}

void VideoStream::borrowedPaint(QOpenGLFunctions &) {
}

void VideoStream::ensureBorrowedRenderer() {
}

void VideoStream::borrowedPaint(Painter &, const QRegion &) {
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0) && !defined DESKTOP_APP_DISABLE_PRIVATE_QT_API
void VideoStream::borrowedPaintOffscreen(QRhi *, QRhiRenderTarget *, QRhiCommandBuffer *) {
}

void VideoStream::borrowedPaintOnscreen(QRhi *, QRhiRenderTarget *, QRhiCommandBuffer *) {
}
#endif

rpl::lifetime &VideoStream::lifetime() {
	return _lifetime;
}

} // namespace Media::View
