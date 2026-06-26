# Makefile for mini-conducted-emission-lisn-test
# Builds the LISN conducted emission test library and test suite
# Reference: CISPR 16-1-2, Paul (2006)

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -Iinclude
LDFLAGS = -lm

# Source files (excluding Lean)
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

# Test files
TEST_SRC = $(wildcard tests/*.c)
TEST_BIN = $(TEST_SRC:.c=.exe)

# Example files
EXAMPLE_SRC = $(wildcard examples/*.c)
EXAMPLE_BIN = $(EXAMPLE_SRC:.c=.exe)

# Library target (static)
LIB = liblisn.a

.PHONY: all clean test examples

all: $(LIB)

$(LIB): $(OBJ)
	ar rcs $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(LIB) $(TEST_BIN)
	@for t in $(TEST_BIN); do echo "Running $$t..."; ./$$t; done

tests/%.exe: tests/%.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -L. -llisn $(LDFLAGS)

examples: $(EXAMPLE_BIN)

examples/%.exe: examples/%.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -L. -llisn $(LDFLAGS)

clean:
	rm -f src/*.o $(LIB) tests/*.exe examples/*.exe
