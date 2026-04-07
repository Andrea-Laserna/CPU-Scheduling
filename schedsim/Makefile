CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
SRC_DIR = src
OBJ_DIR = obj
BIN = schedsim

# List of source files based on the lab requirements
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/process.c \
       $(SRC_DIR)/fcfs.c \
       $(SRC_DIR)/sjf.c \
       $(SRC_DIR)/stcf.c \
       $(SRC_DIR)/rr.c \
       $(SRC_DIR)/mlfq.c \
       $(SRC_DIR)/metrics.c \
       $(SRC_DIR)/gantt.c \
       $(SRC_DIR)/utils.c

# Convert source file names to object file names in the OBJ_DIR
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Default target: Compile the simulator 
all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Rule for object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure the object directory exists
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Remove binaries and object files 
clean:
	rm -rf $(OBJ_DIR) $(BIN)

# Run automated test suite 
test: all
	bash tests/test_suite.sh

.PHONY: all clean test
