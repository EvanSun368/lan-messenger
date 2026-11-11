#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

class MTPUser;

namespace Api {
struct MessageToSend;
} // namespace Api

namespace Main {
class Account;
class Session;
} // namespace Main

namespace lan {
struct Identity;
struct PeerInfo;
} // namespace lan

namespace Lan {

[[nodiscard]] std::filesystem::path IdentityDirectory();
[[nodiscard]] std::optional<lan::Identity> TryLoadIdentity();
[[nodiscard]] lan::Identity SaveOrUpdateIdentity(std::string_view displayName);

[[nodiscard]] MTPUser MakeSelfUser(const lan::Identity &identity);
[[nodiscard]] MTPUser MakePeerUser(const lan::PeerInfo &peer);

// Creates a local Main::Session (no MTP user from Telegram) and starts
// the process-side lan::Node bridge for this session.
void StartLocalSession(
	not_null<Main::Account*> account,
	const lan::Identity &identity);

// Intercept an outgoing compose send. Returns true when the text was
// handled on the LAN path and must not be sent through MTP.
bool TrySendText(
	not_null<Main::Session*> session,
	const Api::MessageToSend &message);

} // namespace Lan
