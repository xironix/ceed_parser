CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O3 -march=native -pthread -I./include -I/opt/homebrew/include -I/usr/local/include -DENABLE_DEBUG
DEBUG_FLAGS = -g -DDEBUG
LDFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lcrypto -lpthread -lsqlite3

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
INCLUDE_DIR = include
DATA_DIR = data

# Common source files for all executables (excluding main entry points)
COMMON_SOURCES = $(filter-out $(SRC_DIR)/main.c $(SRC_DIR)/benchmark.c,$(wildcard $(SRC_DIR)/*.c))
COMMON_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(COMMON_SOURCES))

# Main application
MAIN_SOURCES = $(SRC_DIR)/main.c
MAIN_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(MAIN_SOURCES))
TARGET = $(BIN_DIR)/ceed_parser

# Benchmark application
BENCH_SOURCES = $(SRC_DIR)/benchmark.c
BENCH_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(BENCH_SOURCES))
BENCH_TARGET = $(BIN_DIR)/bench_ceed_parser

# Wordlist files
WORDLISTS = $(wildcard $(DATA_DIR)/*.txt)

# Dependencies
DEPS = $(COMMON_OBJECTS:.o=.d) $(MAIN_OBJECTS:.o=.d) $(BENCH_OBJECTS:.o=.d)

.PHONY: all clean debug benchmarks

all: dirs copy_wordlists $(TARGET) $(BENCH_TARGET)

debug: CFLAGS += $(DEBUG_FLAGS)
debug: all

benchmarks: dirs copy_wordlists $(BENCH_TARGET)

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)
	@mkdir -p $(BIN_DIR)/data

copy_wordlists: dirs
	@echo "Copying wordlist files..."
	@cp -f $(WORDLISTS) $(BIN_DIR)/data/

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET): $(COMMON_OBJECTS) $(MAIN_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BENCH_TARGET): $(COMMON_OBJECTS) $(BENCH_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	rm -f *.o
	rm -f test_*
	rm -f debug_*
	rm -f *.out
	rm -f *.exe
	rm -f *.log
	rm -f *.tmp
	rm -f *.parse
	rm -f *.prof
	rm -f gmon.out
	rm -f perf.data*
	rm -f vgcore.*
	rm -f *.dSYM

-include $(DEPS) 