# Makefile

CC=gcc
CXX=g++

# Include directories
FMT_LIB_DIR=ext/fmt/include
INI_LIB_DIR=ext/headers
JSON_LIB_DIR=ext/headers
ARGS_LIB_DIR=ext/headers

INC=-Isrc/ -I$(FMT_LIB_DIR) -I$(INI_LIB_DIR) -I$(ARGS_LIB_DIR) -I$(JSON_LIB_DIR)
CXXFLAGS=-Wall -O3 -fPIC -std=c++17 $(INC) -DFMT_HEADER_ONLY=1
CXXFLAGS += -DPRINT_ISSUE_LOG 
CXXFLAGS += -DPRINT_RETURN_LOG

# Directories
BUILD_DIR := build
TRACE_DIR := traces

# Output binaries (now in top-level)
TEST_EXE := test
GEN_EXE := generate

# Source files
SRCS = src/bankstate.cc src/channel_state.cc src/command_queue.cc src/common.cc \
       src/configuration.cc src/controller.cc src/dram_system.cc src/hmc.cc \
       src/memory_system.cc src/refresh.cc src/simple_stats.cc src/timing.cc \
       src/logger.cc

TEST_SRC = src/main.cc
GEN_SRC = src/generator.cc

# Object files
OBJS = $(patsubst src/%.cc, $(BUILD_DIR)/%.o, $(SRCS))
TEST_OBJ = $(patsubst src/%.cc, $(BUILD_DIR)/%.o, $(TEST_SRC))
GEN_OBJ = $(patsubst src/%.cpp, $(BUILD_DIR)/%.o, $(GEN_SRC))

.PHONY: all clean

all: $(BUILD_DIR) $(TEST_EXE) $(GEN_EXE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Target: test
$(TEST_EXE): $(TEST_OBJ) $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Target: generate
$(GEN_EXE): $(GEN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile source files
$(BUILD_DIR)/%.o: src/%.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) test generate
