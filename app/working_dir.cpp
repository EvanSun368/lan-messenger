#include "working_dir.h"

#include <fstream>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

std::filesystem::path executable_directory() {
#ifdef _WIN32
	std::vector<wchar_t> buffer(MAX_PATH);
	DWORD n = GetModuleFileNameW(nullptr, buffer.data(), MAX_PATH);
	if (n == 0 || n >= MAX_PATH) {
		return std::filesystem::current_path();
	}
	return std::filesystem::path(std::wstring(buffer.data(), n)).parent_path();
#else
	std::error_code ec;
	auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
	if (ec) {
		return std::filesystem::current_path();
	}
	return path.parent_path();
#endif
}

bool directory_is_writable(const std::filesystem::path &dir) {
	std::error_code ec;
	if (!std::filesystem::is_directory(dir, ec)) {
		return false;
	}
	const auto probe = dir / ".lanlink-write-test";
	{
		std::ofstream out(probe, std::ios::binary | std::ios::trunc);
		if (!out) {
			return false;
		}
		out << 'x';
		if (!out) {
			return false;
		}
	}
	std::filesystem::remove(probe, ec);
	return true;
}

std::filesystem::path resolve_working_dir(int argc, char **argv) {
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i] ? argv[i] : "";
		if (arg == "--dir" && i + 1 < argc) {
			return std::filesystem::absolute(argv[i + 1]);
		}
		if (arg.rfind("--dir=", 0) == 0) {
			return std::filesystem::absolute(arg.substr(6));
		}
	}
	const auto exe = executable_directory();
	if (directory_is_writable(exe)) {
		return exe;
	}
	return std::filesystem::current_path();
}
