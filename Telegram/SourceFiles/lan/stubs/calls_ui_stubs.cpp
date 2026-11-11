/* Stubs for call UI entry points that remain referenced after
 * official calls/*.cpp are excluded (LANLINK_DISABLE_CALLS).
 * These symbols keep the desktop UI linking; they do not start calls.
 */
#include "calls/calls_video_bubble.h"
#include "calls/group/calls_group_call.h"
#include "calls/group/calls_group_common.h"
#include "calls/group/calls_group_invite_controller.h"
#include "calls/group/calls_group_messages.h"
#include "calls/group/calls_group_stars_box.h"
#include "calls/group/ui/calls_group_stars_coloring.h"
#include "data/data_peer.h"
#include "ui/layers/generic_box.h"
#include "ui/rp_widget.h"
#include "ui/text/text_utilities.h"

#include <memory>

namespace Calls {

std::shared_ptr<ParticipantVideoParams> ParseVideoParams(
		const tl::conditional<MTPGroupCallParticipantVideo> &,
		const tl::conditional<MTPGroupCallParticipantVideo> &,
		const std::shared_ptr<ParticipantVideoParams> &) {
	return nullptr;
}

const std::string &GetCameraEndpoint(
		const std::shared_ptr<ParticipantVideoParams> &) {
	static const std::string kEmpty;
	return kEmpty;
}

const std::string &GetScreenEndpoint(
		const std::shared_ptr<ParticipantVideoParams> &) {
	static const std::string kEmpty;
	return kEmpty;
}

bool IsCameraPaused(const std::shared_ptr<ParticipantVideoParams> &) {
	return false;
}

bool IsScreenPaused(const std::shared_ptr<ParticipantVideoParams> &) {
	return false;
}

uint32 GetAdditionalAudioSsrc(
		const std::shared_ptr<ParticipantVideoParams> &) {
	return 0;
}

bool VideoEndpoint::rtmp() const noexcept {
	return false;
}

VideoBubble::VideoBubble(
	not_null<QWidget*> parent,
	not_null<Webrtc::VideoTrack*> track)
: _content(parent)
, _track(track) {
}

void VideoBubble::updateGeometry(
		DragMode,
		QRect,
		QSize,
		QSize) {
}

QRect VideoBubble::geometry() const {
	return _content.geometry();
}

not_null<PeerData*> GroupCall::messagesFrom() const {
	Unexpected("Calls disabled: GroupCall::messagesFrom");
}

Data::GroupCall *GroupCall::lookupReal() const {
	return nullptr;
}

rpl::producer<bool> GroupCall::canManageValue() const {
	return rpl::single(false);
}

rpl::producer<bool> GroupCall::hasVideoWithFramesValue() const {
	return rpl::single(false);
}

void GroupCall::changeVolume(const Group::VolumeRequest &) {
}

void GroupCall::requestVideoQuality(
		const VideoEndpoint &,
		Group::VideoQuality) {
}

not_null<PeerData*> GroupCall::TrackPeer(
		const std::unique_ptr<VideoTrack> &) {
	Unexpected("Calls disabled: GroupCall::TrackPeer");
}

not_null<Webrtc::VideoTrack*> GroupCall::TrackPointer(
		const std::unique_ptr<VideoTrack> &) {
	Unexpected("Calls disabled: GroupCall::TrackPointer");
}

rpl::producer<QSize> GroupCall::TrackSizeValue(
		const std::unique_ptr<VideoTrack> &) {
	return rpl::single(QSize());
}

} // namespace Calls

namespace Calls::Group {

object_ptr<Ui::BoxContent> PrepareInviteToEmptyBox(
		std::shared_ptr<Data::GroupCall>,
		MsgId,
		std::vector<not_null<UserData*>>) {
	return nullptr;
}

object_ptr<Ui::BoxContent> PrepareCreateCallBox(
		not_null<::Window::SessionController*>,
		Fn<void()>,
		MsgId,
		std::vector<not_null<UserData*>>) {
	return nullptr;
}

object_ptr<Ui::BoxContent> MakeVideoStreamStarsBox(
		VideoStreamStarsBoxArgs &&) {
	return nullptr;
}

void Messages::send(TextWithTags, int) {
}

rpl::producer<std::vector<Message>> Messages::listValue() const {
	return rpl::single(std::vector<Message>());
}

rpl::producer<MessageIdUpdate> Messages::idUpdates() const {
	return rpl::never<MessageIdUpdate>();
}

int Messages::reactionsPaidScheduled() const {
	return 0;
}

void Messages::reactionsPaidAdd(int) {
}

void Messages::reactionsPaidScheduledCancel() {
}

void Messages::reactionsPaidSend() {
}

Messages::PaidLocalState Messages::starsLocalState() const {
	return {};
}

void Messages::deleteConfirmed(MessageDeleteRequest) {
}

} // namespace Calls::Group

namespace Calls::Group::Ui {

StarsColoring StarsColoringForCount(
		const std::vector<StarsColoring> &colorings,
		int) {
	return colorings.empty() ? StarsColoring() : colorings.front();
}

int StarsRequiredForMessage(
		const std::vector<StarsColoring> &,
		const TextWithTags &) {
	return 0;
}

object_ptr<RpWidget> VideoStreamStarsLevel(
		not_null<RpWidget*> box,
		const std::vector<StarsColoring> &,
		rpl::producer<int>) {
	return object_ptr<RpWidget>(box);
}

} // namespace Calls::Group::Ui
