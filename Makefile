CC = gcc
VERSION = 1.0.0

# Directories
SERIVCE_DIR = service
CLIENT_DIR = client
OBJ_DIR = obj
BIN_DIR = bin
INSTALL_DIR = /usr/local/bin
RUNTIME_DIR = $(shell echo "/var/run/user/$$(id -u)/papad")

# Compiler flags
OPTIM_FLAGS = -O2
DEBUG_FLAGS = -g
WARN_FLAGS = -Wall -Wextra -Wpedantic -Wformat=2 -Wno-unused-parameter \
             -Wshadow -Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
             -Wredundant-decls -Wnested-externs -Wmissing-include-dirs

CFLAGS = $(WARN_FLAGS) $(OPTIM_FLAGS) $(DEBUG_FLAGS) -I./$(SERIVCE_DIR) -DVERSION=\"$(VERSION)\" \
         $(shell pkg-config --cflags libpipewire-0.3 libspa-0.2 yaml-0.1 sndfile)

LDFLAGS = $(shell pkg-config --libs libpipewire-0.3 libspa-0.2 yaml-0.1 sndfile) -lpthread -lm

# Source and object files
SERVICE_SRCS = $(wildcard $(SERIVCE_DIR)/*.c)
SERVICE_BIN = $(BIN_DIR)/papad
SERVICE_OBJS = $(SERVICE_SRCS:$(SERIVCE_DIR)/%.c=$(OBJ_DIR)/%.o)
CLIENT_SRCS = $(wildcard $(CLIENT_DIR)/*.c)
CLIENT_BIN = $(BIN_DIR)/papa
CLIENT_OBJS = $(CLIENT_SRCS:$(CLIENT_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS = $(SERVICE_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)

# Phony targets
.PHONY: all clean directories install uninstall debug release help

# Default target
all: directories $(SERVICE_BIN) $(CLIENT_BIN)

# Include dependency files
-include $(DEPS)

# Create directories
directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR) $(RUNTIME_DIR)

# Compile source files
$(OBJ_DIR)/%.o: $(SERIVCE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Build service
$(SERVICE_BIN): SERVICE_OBJS
	$(CC) $(SERVICE_OBJS) -o $(SERVICE_BIN) $(LDFLAGS)
	@echo "Build complete: $(SERVICE_BIN)"

# Build client
$(CLIENT_BIN): $(CLIENT_SRCS)
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
	install -m 755 $(SERVICE_BIN) $(DESTDIR)$(INSTALL_DIR)
	install -m 755 $(CLIENT_BIN) $(DESTDIR)$(INSTALL_DIR)

# Uninstall
uninstall:
	rm -f $(DESTDIR)$(INSTALL_DIR)/$(notdir $(SERVICE_BIN))
	rm -f $(DESTDIR)$(INSTALL_DIR)/$(notdir $(CLIENT_BIN))

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
