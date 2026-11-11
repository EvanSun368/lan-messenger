#!/bin/sh
# Apply packaged-Qt / GCC 14 patches to checkouts of the lib_ui and cmake
# submodules. These are not committed as submodule SHA changes, so a fresh
# clone needs this before cmake. Re-running is safe (patch --forward).
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
cd "$root"

apply() {
	dir=$1
	patch=$2
	if patch -p1 -d "$dir" --forward --dry-run --silent < "$patch" >/dev/null 2>&1; then
		patch -p1 -d "$dir" --forward --silent < "$patch" >/dev/null
		echo "applied $patch"
	elif patch -p1 -d "$dir" --reverse --dry-run --silent < "$patch" >/dev/null 2>&1; then
		echo "already applied $patch"
	else
		echo "could not apply $patch to $dir" >&2
		exit 1
	fi
}

apply Telegram/lib_ui Telegram/build/patches/lib_ui-qt611-accessible-orientation.patch
apply cmake Telegram/build/patches/cmake-pango-filter-usr-include.patch
