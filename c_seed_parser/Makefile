CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O3 -march=native -pthread -I./include
DEBUG_FLAGS = -g -DDEBUG
LDFLAGS = -lcrypto -lpthread

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
INCLUDE_DIR = include

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS = $(OBJECTS:.o=.d)

TARGET = $(BIN_DIR)/seed_parser

.PHONY: all clean debug

all: dirs $(TARGET)

debug: CFLAGS += $(DEBUG_FLAGS)
debug: all

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

-include $(DEPS) 