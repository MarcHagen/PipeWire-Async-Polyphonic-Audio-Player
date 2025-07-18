CC = gcc
VERSION = 1.0.0

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TOOLS_DIR = tools
INSTALL_DIR = /usr/local/bin

# Compiler flags
OPTIM_FLAGS = -O2
DEBUG_FLAGS = -g
WARN_FLAGS = -Wall -Wextra -Wpedantic -Wformat=2 -Wno-unused-parameter \
             -Wshadow -Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
             -Wredundant-decls -Wnested-externs -Wmissing-include-dirs

CFLAGS = $(WARN_FLAGS) $(OPTIM_FLAGS) $(DEBUG_FLAGS) \
         -I./$(SRC_DIR) \
         -DVERSION=\"$(VERSION)\" \
         $(shell pkg-config --cflags libpipewire-0.3 libspa-0.2 yaml-0.1 sndfile libmosquitto)

LDFLAGS = $(shell pkg-config --libs libpipewire-0.3 libspa-0.2 yaml-0.1 sndfile libmosquitto) \
          -lpthread -lm

# Source and object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
TOOL_SRCS = $(wildcard $(TOOLS_DIR)/*.c)
TOOL_BINS = $(TOOL_SRCS:$(TOOLS_DIR)/%.c=$(BIN_DIR)/%)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)
TARGET = $(BIN_DIR)/async-multichannel-audio-player

# Phony targets
.PHONY: all clean directories install uninstall debug release help

# Default target
all: directories $(TARGET) $(TOOL_BINS)

# Include dependency files
-include $(DEPS)

# Create directories
directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Link the main target
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Build tools
$(BIN_DIR)/%: $(TOOLS_DIR)/%.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# Debug build
debug: OPTIM_FLAGS = -O0
debug: DEBUG_FLAGS = -g3 -DDEBUG
debug: all

# Release build
release: OPTIM_FLAGS = -O3 -flto
release: DEBUG_FLAGS =
release: CFLAGS += -DNDEBUG
release: all

# Install
install: all
	install -d $(DESTDIR)$(INSTALL_DIR)
	install -m 755 $(TARGET) $(DESTDIR)$(INSTALL_DIR)

# Uninstall
uninstall:
	rm -f $(DESTDIR)$(INSTALL_DIR)/$(notdir $(TARGET))

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	rm -f $(DEPS)

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Build everything (default)"
	@echo "  debug    - Build with debug flags"
	@echo "  release  - Build with optimization flags"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install the program"
	@echo "  uninstall- Remove the installed program"
	@echo "  help     - Show this help message"