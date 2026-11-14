CC = gcc

INC_DIR = include
INC_DIRS = $(shell find $(INC_DIR) -type d)
INC_FLAGS = $(addprefix -I,$(INC_DIRS))

CFLAGS = -Wall -Wextra -Werror -O2 -pthread -g $(INC_FLAGS)

SRC_DIR = src
TEST_DIR = tests
OBJ_DIR = obj
BIN_DIR = bin

MAIN_TARGET = $(BIN_DIR)/avps
TEST_TARGET = $(BIN_DIR)/run_tests

# All project source files
SRCS = $(shell find $(SRC_DIR) -name '*.c')
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/src/%.o, $(SRCS))

# Exclude src/main.c when compiling objects needed by the test runner
LIB_OBJS = $(filter-out $(OBJ_DIR)/src/main.o, $(OBJS))

# Test source files and object files
TEST_SRCS = $(shell find $(TEST_DIR) -name '*.c')
TEST_OBJS = $(patsubst $(TEST_DIR)/%.c, $(OBJ_DIR)/tests/%.o, $(TEST_SRCS))

all: directories $(MAIN_TARGET)

# Compile main binary
$(MAIN_TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Build and execute unit tests
test: directories $(TEST_TARGET)
	@echo "Running unit tests..."
	@./$(TEST_TARGET)

# Link test runner using test objects + non-main project objects
$(TEST_TARGET): $(TEST_OBJS) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Rule for source objects
$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for test objects
$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

directories:
	@mkdir -p $(OBJ_DIR)/src $(OBJ_DIR)/tests
	@mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR)

# Separate command for clearing bin/
fclean: clean
	rm -rf $(BIN_DIR)

re: fclean all

.PHONY: all directories clean fclean test re
