#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace lan {
namespace test {

inline int &failure_count() {
	static int value = 0;
	return value;
}

inline int &check_count() {
	static int value = 0;
	return value;
}

struct Case {
	const char *name;
	void (*fn)();
};

inline std::vector<Case> &registry() {
	static std::vector<Case> cases;
	return cases;
}

struct Register {
	Register(const char *name, void (*fn)()) {
		registry().push_back({name, fn});
	}
};

} // namespace test
} // namespace lan

#define LAN_CHECK(cond) \
	do { \
		++::lan::test::check_count(); \
		if (!(cond)) { \
			++::lan::test::failure_count(); \
			std::cerr << __FILE__ << ":" << __LINE__ \
				<< " CHECK failed: " << #cond << "\n"; \
		} \
	} while (0)

#define LAN_CHECK_EQ(a, b) \
	do { \
		++::lan::test::check_count(); \
		const auto lan_lhs = (a); \
		const auto lan_rhs = (b); \
		if (!(lan_lhs == lan_rhs)) { \
			++::lan::test::failure_count(); \
			std::cerr << __FILE__ << ":" << __LINE__ \
				<< " CHECK_EQ failed: " << lan_lhs \
				<< " != " << lan_rhs << "\n"; \
		} \
	} while (0)

#define LAN_REQUIRE(cond) \
	do { \
		++::lan::test::check_count(); \
		if (!(cond)) { \
			++::lan::test::failure_count(); \
			std::cerr << __FILE__ << ":" << __LINE__ \
				<< " REQUIRE failed: " << #cond << "\n"; \
			throw std::runtime_error("requirement failed"); \
		} \
	} while (0)

#define LAN_TEST(name) \
	void test_##name(); \
	static ::lan::test::Register register_##name(#name, test_##name); \
	void test_##name()
