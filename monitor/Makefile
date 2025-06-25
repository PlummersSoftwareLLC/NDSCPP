# Compiler settings
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -O2 -g3
INCLUDES = -I.
LDFLAGS =

# Libraries needed
LIBS = -lcurl -lfmt -lncursesw

# Binary name
TARGET = ledmon

# Source files
SOURCES = main.cpp content.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Detect platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)
    INCLUDES += -I$(shell brew --prefix)/include/ -I$(shell brew --prefix ncurses)/include/
    LDFLAGS += -L$(shell brew --prefix)/lib/ -L$(shell brew --prefix ncurses)/lib/
endif

# Default target
all: $(TARGET)

# Link the target binary
$(TARGET): $(OBJECTS)
	@echo "Linking $@..."
	@$(CXX) $(LDFLAGS) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compile source files
%.o: %.cpp
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build files
clean:
	@echo "Cleaning build files..."
	@rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
