CXX := g++

ifeq ($(debug), y)
    CXXFLAGS := -g
else
    CXXFLAGS := -O2 -DNDEBUG
endif
CXXFLAGS := $(CXXFLAGS) -Wall -Werror -Wextra -fPIC

ifndef DEPSDIR
    DEPSDIR := $(shell pwd)/../..
endif

INCLUDE := -I.. -I$(DEPSDIR)
LIBS := ../libnetkit.a $(DEPSDIR)/logger/liblogger.a $(DEPSDIR)/threadpool/cpp/libthreadpool.a -lpthread

OBJS := $(patsubst %.cpp, %.o, $(wildcard *.cpp))

TARGET := echo-server-and-client

.PHONY: all clean pre-process post-clean

all: $(TARGET)

$(OBJS): | pre-process

pre-process:
	$(MAKE) DEPSDIR=$(DEPSDIR) -C ..

post-clean:
	$(MAKE) clean DEPSDIR=$(DEPSDIR) -C ..

$(TARGET): $(OBJS) | pre-process
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean: | post-clean
	rm -f $(TARGET) $(OBJS)
