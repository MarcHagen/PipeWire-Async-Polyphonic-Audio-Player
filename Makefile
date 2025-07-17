CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -I./src $(shell pkg-config --cflags libpipewire-0.3 libspa-0.2 yaml-0.1 sndfile)
LDFLAGS = $(shell pkg-config --libs libpipewire-0.3 libspa-0.2 yaml-0.1 sndfile) -lpthread -lm

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TOOLS_DIR = tools

SRCS = $(wildcard $(SRC_DIR)/*.c)
TOOL_SRCS = $(wildcard $(TOOLS_DIR)/*.c)
TOOL_BINS = $(TOOL_SRCS:$(TOOLS_DIR)/%.c=$(BIN_DIR)/%)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/async-audio-player

.PHONY: all clean directories

all: directories $(TARGET) $(TOOL_BINS)

$(BIN_DIR)/%: $(TOOLS_DIR)/%.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)