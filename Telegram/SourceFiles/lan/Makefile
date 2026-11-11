CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -I.
# MinGW / cross-mingw needs Winsock and a modern WINNT for WSAPoll.
ifneq ($(findstring mingw,$(CXX)),)
  CXXFLAGS += -D_WIN32_WINNT=0x0A00
  LDLIBS += -lws2_32
endif
SRCS = identity.cpp protocol.cpp node.cpp lan_ids.cpp
TEST_SRCS = tests/test_main.cpp tests/test_identity.cpp tests/test_protocol.cpp tests/test_node.cpp tests/test_ids.cpp

.PHONY: all test smoke clean

all: lan_tests lan_node

lan_tests: $(SRCS) $(TEST_SRCS) identity.h protocol.h node.h lan_ids.h socket_compat.h tests/check.h
	$(CXX) $(CXXFLAGS) -Itests -o $@ $(TEST_SRCS) $(SRCS) $(LDLIBS)

lan_node: $(SRCS) lan_cli.cpp identity.h protocol.h node.h socket_compat.h
	$(CXX) $(CXXFLAGS) -o $@ lan_cli.cpp $(SRCS) $(LDLIBS)

test: lan_tests
	./lan_tests

smoke: lan_node
	./tests/smoke_two_processes.sh

clean:
	rm -f lan_tests lan_node lan_tests.exe lan_node.exe
