CXX := g++
AR := ar

ifeq ($(debug), y)
    CXXFLAGS := -g
else
    CXXFLAGS := -O2 -DNDEBUG
endif
CXXFLAGS := $(CXXFLAGS) -Wall -Werror -Wextra -fPIC

MODULE_NAME := netkit

INCLUDE :=
LIBS := ./deps/logger/liblogger.a ./deps/threadpool/cpp/libthreadpool.a -lpthread

OBJS := $(patsubst %.cpp, %.o, $(wildcard *.cpp))

TARGET := lib$(MODULE_NAME).a lib$(MODULE_NAME).so

.PHONY: all clean pre-process post-clean

all: $(TARGET)

pre-process:
	$(MAKE) -C deps/threadpool/cpp
	$(MAKE) -C deps/logger

post-clean:
	$(MAKE) clean -C deps/threadpool/cpp
	$(MAKE) clean -C deps/logger

lib$(MODULE_NAME).a: $(OBJS) | pre-process
	$(AR) rc $@ $^

lib$(MODULE_NAME).so: $(OBJS) | pre-process
	$(CXX) -shared -o $@ $^ $(LIBS)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean: | post-clean
	rm -f $(TARGET) $(OBJS)
