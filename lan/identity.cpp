#include "identity.h"

#include "e2e.h"
#include "protocol.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <stdexcept>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

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

std::string sanitize_fingerprint_part(std::string_view value) {
	std::string out;
	out.reserve(value.size());
	for (unsigned char ch : value) {
		if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == ':') {
			out.push_back(static_cast<char>(ch));
		}
	}
	return out;
}

std::string trim_incomplete_utf8(std::string text) {
	if (text.empty()) {
		return text;
	}
	while (!text.empty()
		&& (static_cast<unsigned char>(text.back()) & 0xc0) == 0x80) {
		text.pop_back();
	}
	if (!text.empty()) {
		const auto last = static_cast<unsigned char>(text.back());
		if ((last & 0xc0) == 0xc0) {
			text.pop_back();
		}
	}
	return text;
}

std::string fingerprint_suffix(std::string_view token) {
	std::uint32_t hash = 2166136261u;
	for (unsigned char ch : token) {
		hash ^= ch;
		hash *= 16777619u;
	}
	char buf[5];
	const auto n = std::snprintf(buf, sizeof(buf), "%04x", hash & 0xffff);
	if (n != 4) {
		return "0000";
	}
	return std::string(buf, 4);
}

#ifdef _WIN32

std::string wide_to_utf8(std::wstring_view wide) {
	if (wide.empty()) {
		return {};
	}
	const auto needed = WideCharToMultiByte(
		CP_UTF8,
		0,
		wide.data(),
		static_cast<int>(wide.size()),
		nullptr,
		0,
		nullptr,
		nullptr);
	if (needed <= 0) {
		return {};
	}
	std::string out(static_cast<size_t>(needed), '\0');
	WideCharToMultiByte(
		CP_UTF8,
		0,
		wide.data(),
		static_cast<int>(wide.size()),
		out.data(),
		needed,
		nullptr,
		nullptr);
	return out;
}

std::string read_windows_machine_guid() {
	HKEY key = nullptr;
	const auto opened = RegOpenKeyExW(
		HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Cryptography",
		0,
		KEY_READ | KEY_WOW64_64KEY,
		&key);
	if (opened != ERROR_SUCCESS) {
		return {};
	}
	wchar_t guid[128];
	DWORD size = sizeof(guid);
	DWORD type = 0;
	const auto queried = RegQueryValueExW(
		key,
		L"MachineGuid",
		nullptr,
		&type,
		reinterpret_cast<LPBYTE>(guid),
		&size);
	RegCloseKey(key);
	if (queried != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
		return {};
	}
	const auto chars = size / sizeof(wchar_t);
	std::wstring_view view(guid, chars);
	while (!view.empty() && (view.back() == L'\0' || view.back() == L'\n' || view.back() == L'\r')) {
		view.remove_suffix(1);
	}
	return wide_to_utf8(view);
}

std::string read_windows_volume_id(const std::filesystem::path &dir) {
	std::error_code ec;
	auto probe = dir.empty() ? std::filesystem::current_path(ec) : dir;
	while (!probe.empty() && !std::filesystem::exists(probe, ec)) {
		const auto parent = probe.parent_path();
		if (parent == probe) {
			break;
		}
		probe = parent;
	}
	const auto wide = probe.empty() ? std::wstring() : probe.wstring();
	if (wide.empty()) {
		return {};
	}
	wchar_t volume[MAX_PATH];
	if (!GetVolumePathNameW(wide.c_str(), volume, MAX_PATH)) {
		return {};
	}
	DWORD serial = 0;
	if (!GetVolumeInformationW(
			volume,
			nullptr,
			0,
			&serial,
			nullptr,
			nullptr,
			nullptr,
			0)) {
		return {};
	}
	char buf[9];
	const auto n = std::snprintf(buf, sizeof(buf), "%08x", static_cast<unsigned>(serial));
	if (n <= 0) {
		return {};
	}
	return std::string(buf, static_cast<size_t>(n));
}

std::string read_windows_hostname() {
	wchar_t buf[256];
	DWORD size = static_cast<DWORD>(sizeof(buf) / sizeof(buf[0]));
	if (GetComputerNameExW(ComputerNameDnsHostname, buf, &size) && size > 0) {
		return wide_to_utf8(std::wstring_view(buf, size));
	}
	size = static_cast<DWORD>(sizeof(buf) / sizeof(buf[0]));
	if (GetComputerNameW(buf, &size) && size > 0) {
		return wide_to_utf8(std::wstring_view(buf, size));
	}
	return {};
}

#else

std::string read_linux_machine_id() {
	const char *paths[] = { "/etc/machine-id", "/var/lib/dbus/machine-id" };
	for (const auto *path : paths) {
		std::ifstream in(path);
		if (!in) {
			continue;
		}
		std::string line;
		std::getline(in, line);
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (!line.empty()) {
			return line;
		}
	}
	return {};
}

std::string read_posix_volume_id(const std::filesystem::path &dir) {
	std::error_code ec;
	auto probe = dir.empty() ? std::filesystem::current_path(ec) : dir;
	while (!probe.empty() && !std::filesystem::exists(probe, ec)) {
		const auto parent = probe.parent_path();
		if (parent == probe) {
			break;
		}
		probe = parent;
	}
	if (probe.empty()) {
		return {};
	}
	struct stat st {};
	if (stat(probe.c_str(), &st) != 0) {
		return {};
	}
	return std::to_string(static_cast<unsigned long long>(st.st_dev));
}

std::string read_posix_hostname() {
	char buf[256];
	if (gethostname(buf, sizeof(buf)) != 0) {
		return {};
	}
	buf[sizeof(buf) - 1] = '\0';
	return std::string(buf);
}

#endif

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

bool is_valid_machine_fingerprint(std::string_view fingerprint) {
	if (fingerprint.empty() || fingerprint.size() > 256) {
		return false;
	}
	for (unsigned char ch : fingerprint) {
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

std::string make_machine_fingerprint(
	std::string_view os_machine_id,
	std::string_view volume_id) {
	const auto os = sanitize_fingerprint_part(os_machine_id);
	const auto volume = sanitize_fingerprint_part(volume_id);
	if (os.empty() && volume.empty()) {
		return {};
	}
	return "os=" + os + ";vol=" + volume;
}

std::string current_machine_fingerprint(const std::filesystem::path &dir) {
#ifdef _WIN32
	return make_machine_fingerprint(
		read_windows_machine_guid(),
		read_windows_volume_id(dir));
#else
	return make_machine_fingerprint(
		read_linux_machine_id(),
		read_posix_volume_id(dir));
#endif
}

std::string current_hostname() {
#ifdef _WIN32
	return read_windows_hostname();
#else
	return read_posix_hostname();
#endif
}

std::string default_display_name_from(
	std::string_view hostname,
	std::string_view fallback_token) {
	std::string cleaned;
	cleaned.reserve(hostname.size());
	for (unsigned char ch : hostname) {
		if (!is_control(ch)) {
			cleaned.push_back(static_cast<char>(ch));
		}
	}
	if (cleaned.size() > 64) {
		cleaned.resize(64);
		cleaned = trim_incomplete_utf8(std::move(cleaned));
	}
	const auto name = normalize_display_name(cleaned);
	if (!name.empty()) {
		return name;
	}
	return "LanLink-" + fingerprint_suffix(fallback_token);
}

Identity create_identity(std::string_view display_name) {
	const auto name = normalize_display_name(display_name);
	if (name.empty()) {
		throw std::invalid_argument("invalid display name");
	}
	return Identity{ random_hex_id(), name, {} };
}

void save_identity(const std::filesystem::path &dir, const Identity &identity) {
	if (!is_valid_peer_id(identity.peer_id)
		|| !is_valid_display_name(identity.display_name)) {
		throw std::invalid_argument("invalid identity");
	}
	if (!identity.machine_fingerprint.empty()
		&& !is_valid_machine_fingerprint(identity.machine_fingerprint)) {
		throw std::invalid_argument("invalid identity");
	}
	std::filesystem::create_directories(dir);
	write_single_line_file(dir / "peer_id", identity.peer_id);
	write_single_line_file(dir / "display_name", identity.display_name);
	if (!identity.machine_fingerprint.empty()) {
		write_single_line_file(
			dir / "machine_fingerprint",
			identity.machine_fingerprint);
	}
}

Identity load_identity(const std::filesystem::path &dir) {
	Identity identity;
	identity.peer_id = read_single_line_file(dir / "peer_id");
	identity.display_name = read_single_line_file(dir / "display_name");
	const auto fingerprint_path = dir / "machine_fingerprint";
	if (std::filesystem::exists(fingerprint_path)) {
		identity.machine_fingerprint = read_single_line_file(fingerprint_path);
	}
	if (!is_valid_peer_id(identity.peer_id)
		|| !is_valid_display_name(identity.display_name)
		|| (!identity.machine_fingerprint.empty()
			&& !is_valid_machine_fingerprint(identity.machine_fingerprint))) {
		throw std::runtime_error("corrupt identity in " + dir.string());
	}
	return identity;
}

Identity load_or_create_identity(
	const std::filesystem::path &dir,
	std::string_view display_name) {
	return load_or_create_identity(
		dir,
		display_name,
		current_machine_fingerprint(dir));
}

Identity load_or_create_identity(
	const std::filesystem::path &dir,
	std::string_view display_name,
	std::string_view machine_fingerprint) {
	const auto wanted = normalize_display_name(display_name);
	const auto fingerprint = std::string(machine_fingerprint);
	if (!fingerprint.empty() && !is_valid_machine_fingerprint(fingerprint)) {
		throw std::invalid_argument("invalid machine fingerprint");
	}
	if (std::filesystem::exists(dir / "peer_id")) {
		auto identity = load_identity(dir);
		if (identity.machine_fingerprint.empty()) {
			if (!fingerprint.empty()) {
				identity.machine_fingerprint = fingerprint;
				save_identity(dir, identity);
			}
			return identity;
		}
		if (!fingerprint.empty() && fingerprint != identity.machine_fingerprint) {
			if (wanted.empty()) {
				throw std::invalid_argument("invalid display name");
			}
			discard_e2e_key(dir);
			identity = create_identity(wanted);
			identity.machine_fingerprint = fingerprint;
			save_identity(dir, identity);
			return identity;
		}
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
	identity.machine_fingerprint = fingerprint;
	save_identity(dir, identity);
	return identity;
}

} // namespace lan
