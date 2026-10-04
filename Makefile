CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude -pthread

all: server test_runner

server: src/epoll.cpp
	$(CXX) $(CXXFLAGS) -o server src/epoll.cpp

test_runner: test/load_test.cpp
	$(CXX) $(CXXFLAGS) -o test_runner test/load_test.cpp

clean:
	rm -f server test_runner

.PHONY: all clean