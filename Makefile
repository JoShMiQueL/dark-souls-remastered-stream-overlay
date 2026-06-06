# Cross-compilation Makefile for Linux (MinGW-w64)
# Produces build/Release/dinput8.dll identical in purpose to the MSBuild output.
#
# Prerequisites (Ubuntu/Debian):
#   sudo apt-get install g++-mingw-w64-x86-64
#
# Usage:
#   make            # Release build (default)
#   make debug      # Debug build
#   make clean      # Remove build artifacts

CXX      := x86_64-w64-mingw32-g++-posix
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude \
            -DWIN32_LEAN_AND_MEAN -D_WINDOWS -D_USRDLL
LDFLAGS  := -shared -static-libgcc -static-libstdc++
LDLIBS   := -lws2_32 -lcrypt32 -lpsapi -ladvapi32 -luser32

SRCS := src/dllmain.cpp \
        src/DirectInputProxy.cpp \
        src/DebugConsole.cpp \
        src/MemoryReader.cpp \
        src/WebSocketServer.cpp

# --- Release -----------------------------------------------------------
RELEASE_DIR   := build/Release
RELEASE_OBJ   := $(RELEASE_DIR)/obj
RELEASE_OBJS  := $(patsubst src/%.cpp,$(RELEASE_OBJ)/%.o,$(SRCS))
RELEASE_OUT   := $(RELEASE_DIR)/dinput8.dll
RELEASE_FLAGS := -O2 -DNDEBUG -D_UNICODE -DUNICODE

# --- Debug -------------------------------------------------------------
DEBUG_DIR   := build/Debug
DEBUG_OBJ   := $(DEBUG_DIR)/obj
DEBUG_OBJS  := $(patsubst src/%.cpp,$(DEBUG_OBJ)/%.o,$(SRCS))
DEBUG_OUT   := $(DEBUG_DIR)/dinput8.dll
DEBUG_FLAGS := -g -O0 -D_DEBUG

.PHONY: all release debug clean

all: release

release: $(RELEASE_OUT)

$(RELEASE_OBJ)/%.o: src/%.cpp | $(RELEASE_OBJ)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -c $< -o $@

$(RELEASE_OUT): $(RELEASE_OBJS) | $(RELEASE_DIR)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

debug: $(DEBUG_OUT)

$(DEBUG_OBJ)/%.o: src/%.cpp | $(DEBUG_OBJ)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -c $< -o $@

$(DEBUG_OUT): $(DEBUG_OBJS) | $(DEBUG_DIR)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(RELEASE_DIR) $(RELEASE_OBJ) $(DEBUG_DIR) $(DEBUG_OBJ):
	mkdir -p $@

clean:
	rm -rf build
