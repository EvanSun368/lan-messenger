/* Stub implementations of Calls::Instance and related entry points.
 * Official calls/*.cpp are not compiled when LANLINK_DISABLE_CALLS is on.
 * Dummy complete types let unique_ptr members in the real headers destroy.
 */
#include "calls/calls_instance.h"
#include "calls/calls_box_controller.h"
#include "calls/group/calls_group_common.h"
#include "media/audio/media_audio_track.h"
#include "window/window_unlock_passcode_box.h"
#include "styles/style_calls.h"
#include "styles/style_media_view.h"

#include "ui/text/text_utilities.h"

#include <memory>

#include "tgcalls/VideoCaptureInterface.h"

namespace tgcalls {
class Instance {};
class GroupInstanceCustomImpl {};
enum class State {};
enum class AudioState {};
struct GroupLevelsUpdate {};
struct GroupNetworkState {};
enum class VideoCodecName {};
} // namespace tgcalls

namespace Calls {
struct DhConfig {};
class Call {};
class Panel {};
class GroupCall {};
class Instance::Delegate {};
} // namespace Calls

namespace Calls::Group {
class Panel {};
class ChooseJoinAsProcess {};
class StartRtmpProcess {};
} // namespace Calls::Group

namespace Calls {

Instance::Instance()
: _delegate(nullptr)
, _cachedDhConfig(nullptr)
, _chooseJoinAs(nullptr)
, _startWithRtmp(nullptr) {
}

Instance::~Instance() = default;

void Instance::startOutgoingCall(not_null<UserData*>, StartOutgoingCallArgs) {
}

void Instance::startOrJoinGroupCall(
	std::shared_ptr<Ui::Show>,
	not_null<PeerData*>,
	StartGroupCallArgs) {
}

void Instance::startOrJoinConferenceCall(StartConferenceInfo) {
}

void Instance::startedConferenceReady(
	not_null<GroupCall*>,
	StartConferenceInfo) {
}

void Instance::showStartWithRtmp(
	std::shared_ptr<Ui::Show>,
	not_null<PeerData*>) {
}

void Instance::handleUpdate(not_null<Main::Session*>, const MTPUpdate &) {
}

void Instance::applyGroupCallUpdateChecked(
	not_null<Main::Session*>,
	const MTPUpdate &) {
}

void Instance::showInfoPanel(not_null<Call*>) {
}

void Instance::showInfoPanel(not_null<GroupCall*>) {
}

Call *Instance::currentCall() const {
	return nullptr;
}

rpl::producer<Call*> Instance::currentCallValue() const {
	return rpl::single<Call*>(nullptr);
}

GroupCall *Instance::currentGroupCall() const {
	return nullptr;
}

rpl::producer<GroupCall*> Instance::currentGroupCallValue() const {
	return rpl::single<GroupCall*>(nullptr);
}

bool Instance::inCall() const {
	return false;
}

bool Instance::inGroupCall() const {
	return false;
}

bool Instance::hasVisiblePanel(Main::Session *) const {
	return false;
}

bool Instance::hasActivePanel(Main::Session *) const {
	return false;
}

bool Instance::activateCurrentCall(const QString &) {
	return false;
}

bool Instance::minimizeCurrentActiveCall() {
	return false;
}

bool Instance::toggleFullScreenCurrentActiveCall() {
	return false;
}

bool Instance::closeCurrentActiveCall() {
	return false;
}

void Instance::hidePanelLayers() {
}

auto Instance::getVideoCapture(std::optional<QString>, bool)
	-> std::shared_ptr<tgcalls::VideoCaptureInterface> {
	return std::make_shared<tgcalls::VideoCaptureInterface>();
}

void Instance::requestPermissionsOrFail(Fn<void()> onSuccess, bool) {
	if (onSuccess) {
		onSuccess();
	}
}

void Instance::registerConferenceInvite(
	CallId, not_null<UserData*>, MsgId, bool) {
}

void Instance::unregisterConferenceInvite(
	CallId, not_null<UserData*>, MsgId, bool, bool) {
}

void Instance::showConferenceInvite(not_null<UserData*>, MsgId) {
}

void Instance::declineIncomingConferenceInvites(CallId) {
}

void Instance::declineOutgoingConferenceInvite(
	CallId, not_null<UserData*>, bool) {
}

FnMut<void()> Instance::addAsyncWaiter() {
	return nullptr;
}

void Instance::registerVideoStream(not_null<GroupCall*>) {
}

bool Instance::isSharingScreen() const {
	return false;
}

bool Instance::isQuitPrevent() {
	return false;
}

not_null<Media::Audio::Track*> Instance::ensureSoundLoaded(const QString &) {
	Unexpected("Calls disabled: ensureSoundLoaded");
}

void Instance::playSoundOnce(const QString &) {
}

void Instance::createCall(not_null<UserData*>, CallType, StartOutgoingCallArgs) {
}

void Instance::destroyCall(not_null<Call*>) {
}

void Instance::finishConferenceInvitations(const StartConferenceInfo &) {
}

void Instance::createGroupCall(Group::JoinInfo, const MTPInputGroupCall &) {
}

void Instance::destroyGroupCall(not_null<GroupCall*>) {
}

void Instance::confirmLeaveCurrent(
	std::shared_ptr<Ui::Show>,
	not_null<PeerData*>,
	StartGroupCallArgs,
	Fn<void(StartGroupCallArgs)>) {
}

void Instance::requestPermissionOrFail(Platform::PermissionType, Fn<void()>) {
}

void Instance::refreshDhConfig() {
}

void Instance::refreshServerConfig(not_null<Main::Session*>) {
}

bytes::const_span Instance::updateDhConfig(const MTPmessages_DhConfig &) {
	return {};
}

bool Instance::activateUnconfirmedCall(not_null<UserData*>) {
	return false;
}

void Instance::destroyCurrentCall(Data::GroupCall *, const QString &) {
}

void Instance::handleCallUpdate(not_null<Main::Session*>, const MTPPhoneCall &) {
}

void Instance::handleSignalingData(
	not_null<Main::Session*>,
	const MTPDupdatePhoneCallSignalingData &) {
}

void Instance::handleGroupCallUpdate(
	not_null<Main::Session*>,
	const MTPUpdate &) {
}

void ShowCallsBox(
	not_null<::Window::SessionController*>,
	bool) {
}

void ClearCallsBox(
	not_null<Ui::GenericBox*>,
	not_null<::Window::SessionController*>) {
}

} // namespace Calls

namespace Calls::Group {

void ShowUniqueCaptureOptions(
	std::shared_ptr<Ui::Show>,
	Fn<void(bool withAudio)>) {
}

object_ptr<Ui::RpWidget> MakeRoundActiveLogo(
	not_null<QWidget*> parent,
	const style::icon &,
	const style::margins &) {
	return object_ptr<Ui::RpWidget>(parent);
}

object_ptr<Ui::RpWidget> MakeJoinCallLogo(not_null<QWidget*> parent) {
	return object_ptr<Ui::RpWidget>(parent);
}

void ConferenceCallJoinConfirm(
	not_null<Ui::GenericBox*>,
	std::shared_ptr<Data::GroupCall>,
	UserData *,
	Fn<void(Fn<void()> close)>) {
}

void ShowConferenceCallLinkBox(
	std::shared_ptr<Main::SessionShow>,
	std::shared_ptr<Data::GroupCall>,
	const ConferenceCallLinkArgs &) {
}

void MakeConferenceCall(ConferenceFactoryArgs &&) {
}

object_ptr<Ui::GenericBox> ScreenSharingPrivacyRequestBox() {
	return nullptr;
}

ConferenceCallLinkStyleOverrides DarkConferenceCallLinkStyle() {
	return {};
}

::Window::UnlockPasscodeBoxStyle DarkUnlockPasscodeBoxStyle() {
	return {
		.box = &st::groupCallUnlockBox,
		.close = &st::storiesStealthBoxClose,
		.description = &st::groupCallUnlockDescription,
		.field = &st::groupCallUnlockField,
		.error = &st::groupCallUnlockError,
	};
}

QString ExtractConferenceSlug(const QString &) {
	return {};
}

} // namespace Calls::Group
