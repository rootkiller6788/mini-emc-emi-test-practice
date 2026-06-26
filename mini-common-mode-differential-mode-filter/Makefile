CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -Wpedantic -O2 -g
INCLUDES := -Iinclude
SRC_DIR  := src
TEST_DIR := tests
EX_DIR   := examples
BUILD    := build

SRC      := $(wildcard $(SRC_DIR)/*.c)
TEST_SRC := $(wildcard $(TEST_DIR)/*.c)
EX_SRC   := $(wildcard $(EX_DIR)/*.c)
OBJ      := $(SRC:$(SRC_DIR)/%.c=$(BUILD)/%.o)
TEST_OBJ := $(filter-out $(BUILD)/main.o,$(OBJ))
TESTS    := $(TEST_SRC:$(TEST_DIR)/%.c=$(BUILD)/test_%)
EXES     := $(EX_SRC:$(EX_DIR)/%.c=$(BUILD)/example_%)

.PHONY: all clean test examples

all: $(BUILD) lib test examples

# Shared library
lib: $(BUILD)/libcmdm.so
$(BUILD)/libcmdm.so: $(OBJ)
	$(CC) -shared -o $@ $^ -lm

# Object files
$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -fPIC -c $< -o $@

# Test executables
$(BUILD)/test_%: $(TEST_DIR)/%.c $(TEST_OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(TEST_OBJ) -o $@ -lm

# Example executables
$(BUILD)/example_%: $(EX_DIR)/%.c $(TEST_OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(TEST_OBJ) -o $@ -lm

test: $(TESTS)
	@echo "=== Running Tests ==="
	@failed=0; \
	for t in $(TESTS); do \
		echo "--- $$(basename $$t) ---"; \
		if $$t; then echo "PASS: $$(basename $$t)"; else echo "FAIL: $$(basename $$t)"; failed=$$((failed+1)); fi; \
	done; \
	if [ $$failed -eq 0 ]; then echo "ALL TESTS PASSED"; else echo "$$failed TEST(S) FAILED"; exit 1; fi

examples: $(EXES)
	@echo "=== Running Examples ==="
	@for e in $(EXES); do \
		echo "--- $$(basename $$e) ---"; \
		$$e; \
	done

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.DEFAULT_GOAL := all