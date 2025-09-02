# ===== Directories =====
SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build
BIN_DIR   := bin

# ===== Tools =====
CC := gcc

# ===== Flags & Libs =====
CFLAGS  := -Wall -Wextra -std=c99 -I$(INC_DIR)
LDFLAGS :=
LIBS    := -lGL -lglfw -lm

# ===== Sources / Objects / Deps =====
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)

# ===== Target =====
TARGET := $(BIN_DIR)/main

# Default
all: $(TARGET)

# Link
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LIBS)

# Compile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Create dirs if missing
$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

# Clean / Rebuild
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

rebuild: clean all

# Convenience configs
debug: CFLAGS += -O0 -g
debug: rebuild

release: CFLAGS += -O2 -DNDEBUG
release: rebuild

help:
	@echo "Targets:"
	@echo "  all      -> Build (bin/main)"
	@echo "  clean    -> Remove build/bin"
	@echo "  rebuild  -> Clean + build"
	@echo "  debug    -> O0 + g"
	@echo "  release  -> O2 + NDEBUG"

# Include auto-generated deps
-include $(DEPS)

.PHONY: all clean rebuild debug release help
