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

# Change for DEBUG or RELEASE
CFLAGS	:= -c $(DEBUG_CFLAGS)
LDFLAGS	:= $(DEBUG_LDFLAGS)

INCLUDES	:= -I $(OPEN_ZWAVE_SRC) -I $(OPEN_ZWAVE_SRC)/command_classes/ \
    -I $(OPEN_ZWAVE_SRC)/value_classes/ -I $(OPEN_ZWAVE_SRC)/platform/  \
    -I $(OPEN_ZWAVE)/h/platform/unix -I $(OPEN_ZWAVE)/tinyxml/ \
    -I $(OPEN_ZWAVE)/hidapi/hidapi/ -I $(REDIS_CLIENT)/
LIBS = $(wildcard $(OPEN_ZWAVE)/lib/linux/*.a $(REDIS_CLIENT)/*.a)

%.o : %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -o $@ $<

all: redis-bridge

lib:
	$(MAKE) -C ../build/linux

redis-bridge:	Main.o lib
	$(LD) -o $@ $(LDFLAGS) $< $(LIBS) -pthread -ludev -lboost_thread-mt

clean:
	rm -f redis-bridge Main.o

