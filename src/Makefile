# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99
INCLUDES = -I../include/
LIBS = -lGL -lglfw -lm

# Source files
SOURCES = main.c heightMap.c minimap.c plane.c skybox.c text.c textShowing.c tinyobj_loader.c

# Object files (automatically generated from source files)
OBJECTS = $(SOURCES:.c=.o)

# Target executable
TARGET = main

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Rebuild everything from scratch
rebuild: clean all

# Show help
help:
	@echo "Available targets:"
	@echo "  all      - Build the project (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  rebuild  - Clean and build"
	@echo "  help     - Show this help message"

# Declare phony targets
.PHONY: all clean rebuild debug release install uninstall help