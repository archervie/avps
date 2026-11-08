CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -pthread -g -Iinclude
TARGET = $(BIN_DIR)/avps

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(shell find $(SRC_DIR) -name *.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

all: directories $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

directories:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(BIN_DIR)

.PHONY: all directories clean
