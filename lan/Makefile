CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -I.
TDE2E_PREFIX ?= /workspace/deps
CXXFLAGS += -I$(TDE2E_PREFIX)/include
LDLIBS += -L$(TDE2E_PREFIX)/lib -ltde2e -ltdutils -lcrypto -ldl -lz -lpthread
ifneq ($(findstring mingw,$(CXX)),)
  CXXFLAGS += -D_WIN32_WINNT=0x0A00
  LDLIBS += -lws2_32 -lbcrypt
endif
SRCS = identity.cpp protocol.cpp node.cpp e2e.cpp transfer.cpp group.cpp
TEST_SRCS = tests/test_main.cpp tests/test_identity.cpp tests/test_protocol.cpp tests/test_node.cpp tests/test_e2e.cpp tests/test_transfer.cpp tests/test_group.cpp

.PHONY: all test smoke clean

all: lan_tests lan_node

lan_tests: $(SRCS) $(TEST_SRCS) identity.h protocol.h node.h e2e.h transfer.h group.h socket_compat.h tests/check.h
	$(CXX) $(CXXFLAGS) -Itests -o $@ $(TEST_SRCS) $(SRCS) $(LDLIBS)

lan_node: $(SRCS) lan_cli.cpp identity.h protocol.h node.h e2e.h transfer.h group.h socket_compat.h
	$(CXX) $(CXXFLAGS) -o $@ lan_cli.cpp $(SRCS) $(LDLIBS)

test: lan_tests
	./lan_tests

smoke: lan_node
	./tests/smoke_two_processes.sh

clean:
	rm -f lan_tests lan_node lan_tests.exe lan_node.exe
