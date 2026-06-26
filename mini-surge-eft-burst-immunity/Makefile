# Makefile - mini-surge-eft-burst-immunity
# Surge/EFT/Burst Immunity: IEC 61000-4-4 + IEC 61000-4-5
#
# Targets:
#   make          - build library and tests
#   make test     - run test suite
#   make examples - build all examples
#   make all      - build everything
#   make clean    - remove build artifacts

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g
INCLUDES = -Iinclude
LDFLAGS  = -lm

SRC_DIR  = src
TST_DIR  = tests
EX_DIR   = examples
BLD_DIR  = build

# Source files
SRCS = $(SRC_DIR)/surge_waveform.c        $(SRC_DIR)/surge_protection.c        $(SRC_DIR)/surge_coupling.c        $(SRC_DIR)/surge_burst.c        $(SRC_DIR)/surge_energy.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BLD_DIR)/%.o)

# Library
LIB = $(BLD_DIR)/libsurge.a

# Tests
TEST_SRC = $(TST_DIR)/test_surge.c
TEST_BIN = $(BLD_DIR)/test_surge

# Examples
EX1_SRC = $(EX_DIR)/ex1_surge_protector_design.c
EX2_SRC = $(EX_DIR)/ex2_eft_test_analysis.c
EX3_SRC = $(EX_DIR)/ex3_multistage_coordination.c
EX1_BIN = $(BLD_DIR)/ex1_surge_protector_design
EX2_BIN = $(BLD_DIR)/ex2_eft_test_analysis
EX3_BIN = $(BLD_DIR)/ex3_multistage_coordination

.PHONY: all test examples clean

all: $(LIB) $(TEST_BIN) $(EX1_BIN) $(EX2_BIN) $(EX3_BIN)

# Build directory
$(BLD_DIR):
	mkdir -p $(BLD_DIR)

# Library
$(LIB): $(BLD_DIR) $(OBJS)
	ar rcs $@ $(OBJS)

# Compile source files
$(BLD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Test binary
$(TEST_BIN): $(LIB) $(TEST_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) $(TEST_SRC) -L$(BLD_DIR) -lsurge $(LDFLAGS) -o $@

# Example binaries
$(EX1_BIN): $(LIB) $(EX1_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) $(EX1_SRC) -L$(BLD_DIR) -lsurge $(LDFLAGS) -o $@

$(EX2_BIN): $(LIB) $(EX2_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) $(EX2_SRC) -L$(BLD_DIR) -lsurge $(LDFLAGS) -o $@

$(EX3_BIN): $(LIB) $(EX3_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) $(EX3_SRC) -L$(BLD_DIR) -lsurge $(LDFLAGS) -o $@

# Run test
test: $(TEST_BIN)
	./$(TEST_BIN)

# Build examples
examples: $(EX1_BIN) $(EX2_BIN) $(EX3_BIN)

# Clean
clean:
	rm -rf $(BLD_DIR)
