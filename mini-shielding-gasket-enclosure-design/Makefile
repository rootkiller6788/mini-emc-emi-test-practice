# Makefile for mini-shielding-gasket-enclosure-design
# EMI Shielding, Gasket, and Enclosure Design Library
#
# Targets:
#   make          - build library and tests
#   make test     - build and run tests
#   make examples - build all examples
#   make clean    - remove build artifacts

CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O2 -I./include
LDFLAGS := -lm

SRC_MAT    := src/shielding_material.c
SRC_CORE   := src/shielding_core.c
SRC_GASKET := src/shielding_gasket.c
SRC_ENCL   := src/shielding_enclosure.c
SRC_MEAS   := src/shielding_measurement.c
SRC_APP    := src/shielding_application.c

ALL_SRC := $(SRC_MAT) $(SRC_CORE) $(SRC_GASKET) $(SRC_ENCL) $(SRC_MEAS) $(SRC_APP)
ALL_OBJ := $(ALL_SRC:.c=.o)

TEST_SRC := tests/test_shielding.c
TEST_BIN := test_shielding

EX1_SRC := examples/example_enclosure_design.c
EX2_SRC := examples/example_gasket_selection.c
EX3_SRC := examples/example_multilayer_shield.c
EX1_BIN := example_enclosure_design
EX2_BIN := example_gasket_selection
EX3_BIN := example_multilayer_shield

.PHONY: all test examples clean

all: $(ALL_OBJ) $(TEST_BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BIN): $(ALL_OBJ) $(TEST_SRC)
	$(CC) $(CFLAGS) $(TEST_SRC) $(ALL_OBJ) -o $(TEST_BIN) $(LDFLAGS)

examples: $(EX1_BIN) $(EX2_BIN) $(EX3_BIN)

$(EX1_BIN): $(ALL_OBJ) $(EX1_SRC)
	$(CC) $(CFLAGS) $(EX1_SRC) $(ALL_OBJ) -o $(EX1_BIN) $(LDFLAGS)

$(EX2_BIN): $(ALL_OBJ) $(EX2_SRC)
	$(CC) $(CFLAGS) $(EX2_SRC) $(ALL_OBJ) -o $(EX2_BIN) $(LDFLAGS)

$(EX3_BIN): $(ALL_OBJ) $(EX3_SRC)
	$(CC) $(CFLAGS) $(EX3_SRC) $(ALL_OBJ) -o $(EX3_BIN) $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(ALL_OBJ) $(TEST_BIN) $(EX1_BIN) $(EX2_BIN) $(EX3_BIN)
	rm -f src/*.o
