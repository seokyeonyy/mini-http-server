CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude -pthread

# 서버 소스 파일 목록 (파일이 늘어나면 여기에만 추가)
SRCS = src/main.cpp \
       src/epoll.cpp \
       src/http_Parsing.cpp \
       src/session.cpp \
       src/session_pool.cpp \
	   src/file_cache.cpp

HEADERS = $(wildcard include/*.h)

all: server test_runner

# 소스나 헤더 중 하나라도 바뀌면 다시 빌드 (기본: 디버그용)
server: $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -g -o server $(SRCS)

# 성능 측정용 빌드 (최적화 켬)
bench: $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -O2 -o server $(SRCS)

# 메모리 오류 검사용 빌드 (ASan)
asan: $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -g -fsanitize=address -o server $(SRCS)

test_runner: test/load_test.cpp
	$(CXX) $(CXXFLAGS) -o test_runner test/load_test.cpp

clean:
	rm -f server test_runner

.PHONY: all clean bench asan