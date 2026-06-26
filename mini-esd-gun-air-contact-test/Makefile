# Makefile for mini-esd-gun-air-contact-test
# IEC 61000-4-2 ESD Gun Air/Contact Discharge Test Module
#
# Usage:
#   make          - Build library and tests
#   make test     - Build and run tests
#   make examples - Build all examples
#   make clean    - Remove build artifacts

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -O2 -g
LDFLAGS  := -lm
AR       := ar
ARFLAGS  := rcs

INC_DIR  := include
SRC_DIR  := src
TST_DIR  := tests
EXA_DIR  := examples
BLD_DIR  := build

INCLUDES := -I$(INC_DIR)

# Source files
SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(SRCS:$(SRC_DIR)/%.c=$(BLD_DIR)/%.o)

# Test files
TEST_SRC := $(TST_DIR)/test_esd.c
TEST_BIN := $(BLD_DIR)/test_esd

# Example files
EXAMPLES := $(wildcard $(EXA_DIR)/*.c)
EXA_BINS := $(EXAMPLES:$(EXA_DIR)/%.c=$(BLD_DIR)/%)

# Library
LIBRARY  := $(BLD_DIR)/libesdgun.a

.PHONY: all test examples clean

all: $(LIBRARY) $(TEST_BIN)

$(BLD_DIR):
	mkdir -p $(BLD_DIR)

$(LIBRARY): $(OBJS) | $(BLD_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(BLD_DIR)/%.o: $(SRC_DIR)/%.c | $(BLD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TEST_BIN): $(TEST_SRC) $(LIBRARY) | $(BLD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BLD_DIR) -lesdgun $(LDFLAGS) -o $@

test: $(TEST_BIN)
	@echo "Running ESD gun test suite..."
	./$(TEST_BIN)

$(BLD_DIR)/%: $(EXA_DIR)/%.c $(LIBRARY) | $(BLD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BLD_DIR) -lesdgun $(LDFLAGS) -o $@

examples: $(EXA_BINS)
	@echo "All examples built."

clean:
	rm -rf $(BLD_DIR)
