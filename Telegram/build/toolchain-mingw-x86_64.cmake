# MinGW-w64 toolchain for a packaged Windows cross-build from Linux.
# Prefixes: Qt mingw_64 and the MinGW third-party prefix (MINGW_DEPS_PREFIX).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER /usr/bin/x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres)

if (NOT MINGW_DEPS_PREFIX)
    set(MINGW_DEPS_PREFIX /workspace/mingw-deps)
endif()
set(CMAKE_FIND_ROOT_PATH
    ${MINGW_DEPS_PREFIX}
    /workspace/qt/6.8.3/mingw_64
)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
list(PREPEND CMAKE_PREFIX_PATH ${MINGW_DEPS_PREFIX} /workspace/qt/6.8.3/mingw_64)

# Host CMake packages (tl-expected, range-v3, ...) leak /usr/include into
# MinGW compiles and pull in glibc headers. Only search the MinGW prefix
# and Qt for packages; bundled fallbacks cover expected/ranges/xxhash/lz4.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)
set(CMAKE_FIND_USE_CMAKE_SYSTEM_PACKAGE_REGISTRY FALSE)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY FALSE)

# Linux is case-sensitive; official Windows sources include Shlwapi.h / Windows.h.
# These aliases point at the lowercase MinGW-w64 headers.
set(_lanlink_case_inc /workspace/mingw-deps/include-case)
include_directories(SYSTEM ${_lanlink_case_inc})
string(APPEND CMAKE_C_FLAGS " -isystem ${_lanlink_case_inc}")
string(APPEND CMAKE_CXX_FLAGS " -isystem ${_lanlink_case_inc}")

# windres looks up ICON files relative to cwd (the ninja build dir). Compile
# each .rc from its own directory so "..\art\icon256.ico" resolves.
set(CMAKE_RC_COMPILE_OBJECT
    "${CMAKE_COMMAND} -DSOURCE=<SOURCE> -DOBJECT=<OBJECT> -DDEFINES=<DEFINES> -DINCLUDES=<INCLUDES> -DFLAGS=<FLAGS> -DCMAKE_RC_COMPILER=<CMAKE_RC_COMPILER> -DCMAKE_BINARY_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_CURRENT_LIST_DIR}/windres_compile.cmake")
