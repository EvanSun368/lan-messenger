#include "check.h"

#include <iostream>

int main() {
	int ran = 0;
	for (const auto &test : lan::test::registry()) {
		try {
			test.fn();
			++ran;
		} catch (const std::exception &error) {
			std::cerr << "Test " << test.name << " threw: "
				<< error.what() << "\n";
		}
	}
	std::cout << "ran " << ran << " tests, "
		<< lan::test::check_count() << " checks, "
		<< lan::test::failure_count() << " failures\n";
	return lan::test::failure_count() == 0 ? 0 : 1;
}
