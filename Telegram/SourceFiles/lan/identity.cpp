#include "identity.h"

#include "protocol.h"

#include <cctype>
#include <fstream>
#include <stdexcept>

namespace lan {
namespace {

bool is_control(unsigned char ch) {
	return ch < 0x20 || ch == 0x7f;
}

std::string read_single_line_file(const std::filesystem::path &path) {
	std::ifstream in(path);
	if (!in) {
		throw std::runtime_error("unable to read " + path.string());
	}
	std::string line;
	std::getline(in, line);
	if (!line.empty() && line.back() == '\r') {
		line.pop_back();
	}
	return line;
}

void write_single_line_file(const std::filesystem::path &path, std::string_view value) {
	std::ofstream out(path, std::ios::trunc);
	if (!out) {
		throw std::runtime_error("unable to write " + path.string());
	}
	out << value << '\n';
}

} // namespace

bool is_valid_peer_id(std::string_view peer_id) {
	return from_hex(peer_id).value_or(std::vector<uint8_t>{}).size() == 16
		&& peer_id.size() == 32;
}

bool is_valid_display_name(std::string_view name) {
	if (name.empty() || name.size() > 64) {
		return false;
	}
	for (unsigned char ch : name) {
		if (is_control(ch)) {
			return false;
		}
	}
	return true;
}

std::string normalize_display_name(std::string_view name) {
	size_t begin = 0;
	size_t end = name.size();
	while (begin < end && std::isspace(static_cast<unsigned char>(name[begin]))) {
		++begin;
	}
	while (end > begin && std::isspace(static_cast<unsigned char>(name[end - 1]))) {
		--end;
	}
	std::string trimmed(name.substr(begin, end - begin));
	if (!is_valid_display_name(trimmed)) {
		return {};
	}
	return trimmed;
}

Identity create_identity(std::string_view display_name) {
	const auto name = normalize_display_name(display_name);
	if (name.empty()) {
		throw std::invalid_argument("invalid display name");
	}
	return Identity{random_hex_id(), name};
}

void save_identity(const std::filesystem::path &dir, const Identity &identity) {
	if (!is_valid_peer_id(identity.peer_id)
		|| !is_valid_display_name(identity.display_name)) {
		throw std::invalid_argument("invalid identity");
	}
	std::filesystem::create_directories(dir);
	write_single_line_file(dir / "peer_id", identity.peer_id);
	write_single_line_file(dir / "display_name", identity.display_name);
}

Identity load_identity(const std::filesystem::path &dir) {
	Identity identity;
	identity.peer_id = read_single_line_file(dir / "peer_id");
	identity.display_name = read_single_line_file(dir / "display_name");
	if (!is_valid_peer_id(identity.peer_id)
		|| !is_valid_display_name(identity.display_name)) {
		throw std::runtime_error("corrupt identity in " + dir.string());
	}
	return identity;
}

Identity load_or_create_identity(
	const std::filesystem::path &dir,
	std::string_view display_name) {
	const auto wanted = normalize_display_name(display_name);
	if (std::filesystem::exists(dir / "peer_id")) {
		auto identity = load_identity(dir);
		if (!wanted.empty() && wanted != identity.display_name) {
			identity.display_name = wanted;
			save_identity(dir, identity);
		}
		return identity;
	}
	if (wanted.empty()) {
		throw std::invalid_argument("invalid display name");
	}
	auto identity = create_identity(wanted);
	save_identity(dir, identity);
	return identity;
}

} // namespace lan
