# Compiler settings
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -O2
INCLUDES = -I. -I..
SYSTEM_INCLUDES =
LDFLAGS =

# Libraries needed
LIBS = -lgtest_main -lgtest -lcpr -lcurl -lpthread

# Binary name
TARGET = tests

# Source files
SOURCES = tests.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Detect platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)
    # macOS-specific settings using Homebrew
    BREW_PREFIX := $(shell brew --prefix)
    SYSTEM_INCLUDES += -isystem $(BREW_PREFIX)/include
    LDFLAGS += -L$(BREW_PREFIX)/lib
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
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $(SYSTEM_INCLUDES) -c $< -o $@

# Clean build files
clean:
	@echo "Cleaning build files..."
	@rm -f $(OBJECTS) $(TARGET)

# Run the tests
test: $(TARGET)
	@echo "Running tests..."
	@./$(TARGET)

# Install dependencies on macOS
install-deps-mac:
	@echo "Installing dependencies via Homebrew..."
	@brew install googletest cpr

.PHONY: all clean test install-deps-mac
