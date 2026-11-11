#include "lan/bridge.h"

#include "lan/identity.h"
#include "lan/lan_ids.h"
#include "lan/lan_mode.h"
#include "lan/node.h"

#include "api/api_common.h"
#include "base/timer.h"
#include "base/unixtime.h"
#include "data/data_changes.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "settings.h"

#include <QtCore/QDir>
#include <QtCore/QString>

#include <map>
#include <string>

namespace Lan {
namespace {

[[nodiscard]] QString Utf8(std::string_view text) {
	return QString::fromUtf8(text.data(), int(text.size()));
}

[[nodiscard]] MTPUser MakeUser(
		uint64 userId,
		const QString &firstName,
		bool self) {
	using Flag = MTPDuser::Flag;
	const auto flags = Flag::f_first_name
		| Flag::f_access_hash
		| (self ? Flag::f_self : Flag::f_contact);
	return MTP_user(
		MTP_flags(flags),
		MTP_long(userId),
		MTP_long(1), // access_hash
		MTP_string(firstName),
		MTPstring(), // last_name
		MTPstring(), // username
		MTPstring(), // phone
		MTPUserProfilePhoto(),
		MTPUserStatus(),
		MTPint(), // bot_info_version
		MTPVector<MTPRestrictionReason>(),
		MTPstring(), // bot_inline_placeholder
		MTPstring(), // lang_code
		MTPEmojiStatus(),
		MTPVector<MTPUsername>(),
		MTPRecentStory(),
		MTPPeerColor(), // color
		MTPPeerColor(), // profile_color
		MTPint(), // bot_active_users
		MTPlong(), // bot_verification_icon
		MTPlong(), // send_paid_messages_stars
		MTPlong()); // linked_community_id
}

[[nodiscard]] MTPUser MakeSelfUser(const lan::Identity &identity);
[[nodiscard]] MTPUser MakePeerUser(const lan::PeerInfo &peer);

std::map<Main::Session*, class Bridge*> &Bridges();

class Bridge final {
public:
	Bridge(not_null<Main::Session*> session, lan::Identity identity)
	: _session(session)
	, _identity(std::move(identity))
	, _node(MakeConfig(_identity)) {
		remember(_identity.peer_id, lan::user_id_from_peer_id(_identity.peer_id));
		_node.start();
		_timer.setCallback([=] { poll(); });
		_timer.callEach(100);
		poll();
	}

	~Bridge() {
		Bridges().erase(_session);
		_timer.cancel();
		try {
			_node.stop();
		} catch (...) {
		}
	}

	bool sendText(
			not_null<PeerData*> peer,
			const TextWithEntities &text) {
		const auto user = peer->asUser();
		if (!user) {
			return false;
		}
		const auto userId = peerToUser(user->id).bare;
		const auto i = _userToPeer.find(userId);
		if (i == end(_userToPeer)) {
			return false;
		}
		const auto history = _session->data().history(peer);
		history->clearFolder();
		const auto item = history->addNewLocalMessage({
			.id = _session->data().nextLocalMessageId(),
			.flags = MessageFlag::Outgoing,
			.from = _session->userPeerId(),
			.date = base::unixtime::now(),
		}, text, MTP_messageMediaEmpty());
		(void)item;
		try {
			_node.send_text(i->second, text.text.toUtf8().toStdString());
		} catch (const std::exception &error) {
			LOG(("LanLink: send_text failed: %1").arg(
				QString::fromUtf8(error.what())));
		}
		_session->data().sendHistoryChangeNotifications();
		_session->changes().historyUpdated(
			history,
			Data::HistoryUpdate::Flag::MessageSent);
		return true;
	}

private:
	[[nodiscard]] static lan::NodeConfig MakeConfig(const lan::Identity &identity) {
		lan::NodeConfig config;
		config.identity = identity;
		config.bind_address = "0.0.0.0";
		return config;
	}

	void remember(const std::string &peerId, uint64 userId) {
		if (!peerId.empty() && userId) {
			_peerToUser[peerId] = userId;
			_userToPeer[userId] = peerId;
		}
	}

	void poll() {
		try {
			_node.step(std::chrono::milliseconds(0));
		} catch (const std::exception &error) {
			LOG(("LanLink: node step failed: %1").arg(
				QString::fromUtf8(error.what())));
			return;
		}
		for (const auto &peer : _node.take_peer_events()) {
			ensurePeer(peer);
		}
		for (const auto &text : _node.take_texts()) {
			injectIncoming(text);
		}
		(void)_node.take_acks();
	}

	void ensurePeer(const lan::PeerInfo &info) {
		const auto userId = lan::user_id_from_peer_id(info.peer_id);
		if (!userId) {
			return;
		}
		remember(info.peer_id, userId);
		const auto user = _session->data().processUser(MakePeerUser(info));
		const auto history = _session->data().history(user);
		history->clearFolder();
		history->updateChatListExistence();
	}

	void injectIncoming(const lan::IncomingText &text) {
		auto userId = uint64(0);
		const auto i = _peerToUser.find(text.from_peer_id);
		if (i != end(_peerToUser)) {
			userId = i->second;
		} else {
			userId = lan::user_id_from_peer_id(text.from_peer_id);
			if (!userId) {
				return;
			}
			remember(text.from_peer_id, userId);
		}
		lan::PeerInfo info;
		info.peer_id = text.from_peer_id;
		info.display_name = text.from_name.empty()
			? text.from_peer_id
			: text.from_name;
		const auto user = _session->data().processUser(MakePeerUser(info));
		const auto history = _session->data().history(user);
		history->clearFolder();
		const auto date = text.timestamp_ms
			? TimeId(text.timestamp_ms / 1000)
			: base::unixtime::now();
		history->addNewLocalMessage({
			.id = _session->data().nextLocalMessageId(),
			.from = user->id,
			.date = date,
		}, TextWithEntities{ Utf8(text.body) }, MTP_messageMediaEmpty());
		_session->data().sendHistoryChangeNotifications();
	}

	const not_null<Main::Session*> _session;
	const lan::Identity _identity;
	lan::Node _node;
	base::Timer _timer;
	std::map<std::string, uint64> _peerToUser;
	std::map<uint64, std::string> _userToPeer;
};

std::map<Main::Session*, Bridge*> &Bridges() {
	static std::map<Main::Session*, Bridge*> map;
	return map;
}

MTPUser MakeSelfUser(const lan::Identity &identity) {
	return MakeUser(
		lan::user_id_from_peer_id(identity.peer_id),
		Utf8(identity.display_name),
		true);
}

MTPUser MakePeerUser(const lan::PeerInfo &peer) {
	return MakeUser(
		lan::user_id_from_peer_id(peer.peer_id),
		Utf8(peer.display_name),
		false);
}

} // namespace

std::filesystem::path IdentityDirectory() {
	auto dir = cWorkingDir();
	if (dir.isEmpty()) {
		dir = QDir::homePath() + u"/.LanLink/"_q;
	}
	const auto path = QDir(dir).filePath(QString::fromUtf8(kIdentityFolder));
	QDir().mkpath(path);
#ifdef Q_OS_WIN
	return std::filesystem::path(path.toStdWString());
#else
	return std::filesystem::u8path(path.toUtf8().constData());
#endif
}

std::optional<lan::Identity> TryLoadIdentity() {
	try {
		return lan::load_identity(IdentityDirectory());
	} catch (...) {
		return std::nullopt;
	}
}

lan::Identity SaveOrUpdateIdentity(std::string_view displayName) {
	return lan::load_or_create_identity(IdentityDirectory(), displayName);
}

void StartLocalSession(
		not_null<Main::Account*> account,
		const lan::Identity &identity) {
	Expects(Lan::kServerless);
	if (account->sessionExists()) {
		return;
	}
	account->createSession(MakeSelfUser(identity));
	const auto session = &account->session();
	auto *bridge = session->lifetime().make_state<Bridge>(session, identity);
	Bridges()[session] = bridge;
}

bool TrySendText(
		not_null<Main::Session*> session,
		const Api::MessageToSend &message) {
	const auto i = Bridges().find(session);
	if (i == end(Bridges())) {
		return false;
	}
	const auto history = message.action.history;
	const auto text = TextWithEntities{ message.textWithTags.text };
	if (text.text.trimmed().isEmpty()) {
		return true;
	}
	return i->second->sendText(history->peer, text);
}

} // namespace Lan
