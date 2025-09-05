SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build
BIN_DIR   := bin

CC := gcc

CFLAGS  := -std=c99 -I$(INC_DIR) -w
LIBS    := -Llib -lGL -lglfw -lm -L./lib -l:libdarknet.so -Wl,-rpath=./lib

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)

TARGET := $(BIN_DIR)/main

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)/main

rebuild: clean all

debug: CFLAGS += -O0 -g
debug: rebuild

release: CFLAGS += -O2 -DNDEBUG
release: rebuild

-include $(DEPS)

.PHONY: all clean rebuild debug release help
