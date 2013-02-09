#
# Makefile for OpenzWave Redis bridge
# Carl Leiby

# GNU make only
# requires libudev-dev

.SUFFIXES:	.cpp .o .a .s

OPEN_ZWAVE := ../open-zwave/cpp
OPEN_ZWAVE_SRC := $(OPEN_ZWAVE)/src
REDIS_CLIENT := ../redis-cplusplus-client

CC     := $(CROSS_COMPILE)gcc
CXX    := $(CROSS_COMPILE)g++
LD     := $(CROSS_COMPILE)g++
AR     := $(CROSS_COMPILE)ar rc
RANLIB := $(CROSS_COMPILE)ranlib

DEBUG_CFLAGS    := -Wall -Wno-format -g -DDEBUG
RELEASE_CFLAGS  := -Wall -Wno-unknown-pragmas -Wno-format -O3

DEBUG_LDFLAGS	:= -g
UNAME := $(shell uname)

# Change for DEBUG or RELEASE

ifeq ($(UNAME), Darwin)
ARCH :=  -arch x86_64
CFLAGS	:= -c -DDARWIN $(DEBUG_CFLAGS) $(ARCH)
SYSTEM := mac
OTHER_LIBS := -lboost_thread-mt -lboost_system-mt -framework IOKit -framework CoreFoundation $(ARCH)
endif
ifeq ($(UNAME), Linux)
CFLAGS	:= -c $(DEBUG_CFLAGS)
SYSTEM := linux
OTHER_LIBS := -lboost_thread-mt -ludev
endif

LDFLAGS	:= $(DEBUG_LDFLAGS)

INCLUDES	:= -I $(OPEN_ZWAVE_SRC) -I $(OPEN_ZWAVE_SRC)/command_classes/ \
    -I $(OPEN_ZWAVE_SRC)/value_classes/ -I $(OPEN_ZWAVE_SRC)/platform/  \
    -I $(OPEN_ZWAVE)/h/platform/unix -I $(OPEN_ZWAVE)/tinyxml/ \
    -I $(OPEN_ZWAVE)/hidapi/hidapi/ -I $(REDIS_CLIENT)/
LIBS = $(wildcard $(OPEN_ZWAVE)/lib/$(SYSTEM)/*.a $(REDIS_CLIENT)/*.a)

%.o : %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -o $@ $<

all: redis-bridge

lib:
	$(MAKE) -C $(OPEN_ZWAVE)/build/$(SYSTEM)

redis-bridge:	Main.o lib
	$(LD) -o $@ $(LDFLAGS) $< $(LIBS) -pthread $(OTHER_LIBS)

clean:
	rm -f redis-bridge Main.o

