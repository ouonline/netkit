CXX := g++
AR := ar

ifeq ($(debug), y)
    CXXFLAGS := -g
else
    CXXFLAGS := -O2 -DNDEBUG
endif
CXXFLAGS := $(CXXFLAGS) -Wall -Werror -Wextra -fPIC

ifndef DEPSDIR
    DEPSDIR := $(shell pwd)/..
endif

MODULE_NAME := netkit

INCLUDE := -I$(DEPSDIR)
LIBS := $(DEPSDIR)/logger/liblogger.a $(DEPSDIR)/threadpool/cpp/libthreadpool.a -lpthread

OBJS := $(patsubst %.cpp, %.o, $(wildcard *.cpp))

TARGET := lib$(MODULE_NAME).a lib$(MODULE_NAME).so

.PHONY: all clean pre-process post-clean

all: $(TARGET)

$(OBJS): | pre-process

pre-process:
	d=$(DEPSDIR)/threadpool; if ! [ -d $$d ]; then git clone https://github.com/ouonline/threadpool.git $$d; fi
	d=$(DEPSDIR)/logger; if ! [ -d $$d ]; then git clone https://github.com/ouonline/logger.git $$d; fi
	d=$(DEPSDIR)/utils; if ! [ -d $$d ]; then git clone https://github.com/ouonline/utils.git $$d; fi
	$(MAKE) DEPSDIR=$(DEPSDIR) -C $(DEPSDIR)/threadpool/cpp
	$(MAKE) DEPSDIR=$(DEPSDIR) -C $(DEPSDIR)/logger

post-clean:
	$(MAKE) clean DEPSDIR=$(DEPSDIR) -C $(DEPSDIR)/threadpool/cpp
	$(MAKE) clean DEPSDIR=$(DEPSDIR) -C $(DEPSDIR)/logger

lib$(MODULE_NAME).a: $(OBJS)
	$(AR) rc $@ $^

lib$(MODULE_NAME).so: $(OBJS)
	$(CXX) -shared -o $@ $^ $(LIBS)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean: | post-clean
	rm -f $(TARGET) $(OBJS)
