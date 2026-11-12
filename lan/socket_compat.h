#pragma once

// Thin POSIX / Winsock shim used by lan::Node. Socket handles are not ints
// on 64-bit Windows, so every descriptor in node.cpp is a Socket.

#include <cstddef>
#include <stdexcept>
#include <string>

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

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>

using Socket = SOCKET;
inline constexpr Socket kInvalidSocket = INVALID_SOCKET;

using ssize_type = int;

inline void ensure_sockets() {
	static const bool ready = [] {
		WSADATA data{};
		if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
			throw std::runtime_error("WSAStartup failed");
		}
		return true;
	}();
	(void)ready;
}

inline int socket_error() {
	return ::WSAGetLastError();
}

inline bool error_interrupted(int err = socket_error()) {
	return err == WSAEINTR;
}

inline bool error_would_block(int err = socket_error()) {
	return err == WSAEWOULDBLOCK || err == WSAEINTR;
}

inline bool error_connect_pending(int err = socket_error()) {
	// A non-blocking connect reports WSAEWOULDBLOCK, not WSAEINPROGRESS.
	return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEINTR;
}

inline void throw_socket_error(const char *what) {
	throw std::runtime_error(std::string(what) + ": " + std::to_string(socket_error()));
}

inline void close_socket(Socket &fd) {
	if (fd != kInvalidSocket) {
		::closesocket(fd);
		fd = kInvalidSocket;
	}
}

inline void set_nonblock(Socket fd) {
	u_long yes = 1;
	if (::ioctlsocket(fd, FIONBIO, &yes) != 0) {
		throw_socket_error("ioctlsocket FIONBIO");
	}
}

inline int poll_sockets(pollfd *fds, unsigned count, int timeout_ms) {
	return ::WSAPoll(fds, count, timeout_ms);
}

inline int set_sockopt(
		Socket fd,
		int level,
		int name,
		const void *value,
		socklen_t size) {
	return ::setsockopt(fd, level, name, static_cast<const char *>(value), size);
}

inline int get_sockopt(
		Socket fd,
		int level,
		int name,
		void *value,
		socklen_t *size) {
	return ::getsockopt(fd, level, name, static_cast<char *>(value), size);
}

inline ssize_type socket_send(Socket fd, const void *buf, size_t n) {
	return ::send(fd, static_cast<const char *>(buf), static_cast<int>(n), 0);
}

inline ssize_type socket_recv(Socket fd, void *buf, size_t n) {
	return ::recv(fd, static_cast<char *>(buf), static_cast<int>(n), 0);
}

inline ssize_type socket_sendto(
		Socket fd,
		const void *buf,
		size_t n,
		const sockaddr *addr,
		socklen_t addrlen) {
	return ::sendto(
		fd,
		static_cast<const char *>(buf),
		static_cast<int>(n),
		0,
		addr,
		addrlen);
}

inline ssize_type socket_recvfrom(
		Socket fd,
		void *buf,
		size_t n,
		sockaddr *addr,
		socklen_t *addrlen) {
	return ::recvfrom(
		fd,
		static_cast<char *>(buf),
		static_cast<int>(n),
		0,
		addr,
		addrlen);
}

#else // !_WIN32

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

using Socket = int;
inline constexpr Socket kInvalidSocket = -1;

using ssize_type = ssize_t;

inline void ensure_sockets() {}

inline int socket_error() {
	return errno;
}

inline bool error_interrupted(int err = socket_error()) {
	return err == EINTR;
}

inline bool error_would_block(int err = socket_error()) {
	return err == EAGAIN || err == EWOULDBLOCK;
}

inline bool error_connect_pending(int err = socket_error()) {
	return err == EINPROGRESS || err == EINTR;
}

inline void throw_socket_error(const char *what) {
	throw std::runtime_error(std::string(what) + ": " + std::strerror(socket_error()));
}

inline void close_socket(Socket &fd) {
	if (fd != kInvalidSocket) {
		::close(fd);
		fd = kInvalidSocket;
	}
}

inline void set_nonblock(Socket fd) {
	const int flags = ::fcntl(fd, F_GETFL, 0);
	if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		throw_socket_error("fcntl O_NONBLOCK");
	}
}

inline int poll_sockets(pollfd *fds, unsigned count, int timeout_ms) {
	return ::poll(fds, count, timeout_ms);
}

inline int set_sockopt(
		Socket fd,
		int level,
		int name,
		const void *value,
		socklen_t size) {
	return ::setsockopt(fd, level, name, value, size);
}

inline int get_sockopt(
		Socket fd,
		int level,
		int name,
		void *value,
		socklen_t *size) {
	return ::getsockopt(fd, level, name, value, size);
}

inline ssize_type socket_send(Socket fd, const void *buf, size_t n) {
	return ::send(fd, buf, n, 0);
}

inline ssize_type socket_recv(Socket fd, void *buf, size_t n) {
	return ::recv(fd, buf, n, 0);
}

inline ssize_type socket_sendto(
		Socket fd,
		const void *buf,
		size_t n,
		const sockaddr *addr,
		socklen_t addrlen) {
	return ::sendto(fd, buf, n, 0, addr, addrlen);
}

inline ssize_type socket_recvfrom(
		Socket fd,
		void *buf,
		size_t n,
		sockaddr *addr,
		socklen_t *addrlen) {
	return ::recvfrom(fd, buf, n, 0, addr, addrlen);
}

#endif

inline bool valid_socket(Socket fd) {
	return fd != kInvalidSocket;
}


// Official-style LAN transfer sockets: disable Nagle and enlarge buffers
// so a pipelined file send can fill a 1 Gbps link instead of stalling
// on delayed ACKs and the default 64 KiB window.
inline void configure_tcp_socket(Socket fd) {
	int one = 1;
	set_sockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	int buf = 1024 * 1024;
	set_sockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
	set_sockopt(fd, SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));
}
