CXX := g++

ifeq ($(debug), y)
    CXXFLAGS := -g
else
    CXXFLAGS := -O2 -DNDEBUG
endif
CXXFLAGS := $(CXXFLAGS) -Wall -Werror -Wextra -fPIC

INCLUDE := -I..
LIBS := ../libnetkit.a ../deps/logger/liblogger.a ../deps/threadpool/cpp/libthreadpool.a -lpthread

OBJS := $(patsubst %.cpp, %.o, $(wildcard *.cpp))

TARGET := echo-server-and-client

.PHONY: all clean pre-process post-clean

all: $(TARGET)

pre-process:
	$(MAKE) -C ..

post-clean:
	$(MAKE) clean -C ..

$(TARGET): $(OBJS) | pre-process
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean: | post-clean
	rm -f $(TARGET) $(OBJS)
