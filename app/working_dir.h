#pragma once

#include <filesystem>
#include <string>

// Portable layout: if the executable directory is writable, that is the
// working directory (identity, downloads/, state/, history/). Otherwise
// the current directory is used. --dir overrides both.
std::filesystem::path resolve_working_dir(int argc, char **argv);
std::filesystem::path executable_directory();
bool directory_is_writable(const std::filesystem::path &dir);
