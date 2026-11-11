CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -I.
SRCS = identity.cpp protocol.cpp node.cpp
TEST_SRCS = tests/test_main.cpp tests/test_identity.cpp tests/test_protocol.cpp tests/test_node.cpp

.PHONY: all test smoke clean

all: lan_tests lan_node

lan_tests: $(SRCS) $(TEST_SRCS) identity.h protocol.h node.h tests/check.h
	$(CXX) $(CXXFLAGS) -Itests -o $@ $(TEST_SRCS) $(SRCS)

lan_node: $(SRCS) lan_cli.cpp identity.h protocol.h node.h
	$(CXX) $(CXXFLAGS) -o $@ lan_cli.cpp $(SRCS)

test: lan_tests
	./lan_tests

smoke: lan_node
	./tests/smoke_two_processes.sh

clean:
	rm -f lan_tests lan_node
