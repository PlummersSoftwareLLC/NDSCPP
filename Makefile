SHELL:=/bin/bash

CC=clang++
CFLAGS=-std=c++2a -g3 -O3 -Ieffects -I/opt/homebrew/include
LDFLAGS=
LIBS=-lpthread -lz -lavformat -lavcodec -lavutil -lswscale -lswresample

DEPFLAGS=-MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

SOURCES=main.cpp palette.cpp
EXECUTABLE=ndscpp
DEPDIR=.deps
OBJECTS:=$(SOURCES:.cpp=.o)
DEPFILES:=$(SOURCES:%.cpp=$(DEPDIR)/%.d)

define helptext =
Makefile for ndscpp

Usage:
	help	Show this help text
	all	Build the ndscpp application
	force	Force a rebuild of all files
	clean	Remove all build artifacts

Examples:
	$$ make all

endef

# Detect platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)
    CFLAGS += -I/opt/homebrew/include/
    LDFLAGS += -L/opt/homebrew/lib
else
    CFLAGS += -I/usr/include/
    LDFLAGS += -L/usr/lib/
endif

.PHONY: help
help:; @ $(info $(helptext)) :

.PHONY: all
.PHONY: phony
.PHONY: force
.PHONY: clean

all: $(EXECUTABLE)
force: all

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

%.o: %.cpp $(DEPDIR)/%.d | $(DEPDIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(DEFINES) -c $< -o $@

$(DEPDIR): ; @mkdir -p $@

$(DEPFILES):

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) $(DEPFILES)

include $(wildcard $(DEPFILES))
