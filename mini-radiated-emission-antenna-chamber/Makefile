# Makefile for mini-radiated-emission-antenna-chamber
# Radiated Emission Antenna Chamber Theory and Measurement Library
#
# Reference: Paul (2006), Balanis (2016), CISPR 16-1-4:2019
#
# Targets:
#   make          - build library and tests
#   make test     - build and run tests
#   make examples - build all examples
#   make clean    - remove build artifacts

CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O2 -I./include -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable
LDFLAGS := -lm

AF_SRC       := src/antenna_factor.c
ANT_SRC      := src/antenna_types.c
FS_SRC       := src/field_strength.c
NSA_SRC      := src/site_attenuation.c
SCAN_SRC     := src/emission_scan.c
CHAMBER_SRC  := src/chamber_qualify.c
FIELD_SRC    := src/near_far_field.c
LIMITS_SRC   := src/standard_limits.c

ALL_SRC := $(AF_SRC) $(ANT_SRC) $(FS_SRC) $(NSA_SRC) $(SCAN_SRC) \
           $(CHAMBER_SRC) $(FIELD_SRC) $(LIMITS_SRC)
ALL_OBJ := $(ALL_SRC:.c=.o)

TEST_SRC := tests/test_radiated_emission.c
TEST_BIN := test_radiated_emission.exe

EX1_SRC := examples/example_cispr32_test.c
EX2_SRC := examples/example_nsa_validation.c
EX3_SRC := examples/example_near_far_field.c
EX1_BIN := example_cispr32_test.exe
EX2_BIN := example_nsa_validation.exe
EX3_BIN := example_near_far_field.exe

BENCH_SRC := benches/bench_chamber.c
BENCH_BIN := bench_chamber.exe
DEMO_SRC  := demos/demo_scan.c
DEMO_BIN  := demo_scan.exe

.PHONY: all test examples clean bench demo

all: $(ALL_OBJ) $(TEST_BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BIN): $(ALL_OBJ) $(TEST_SRC)
	$(CC) $(CFLAGS) $(TEST_SRC) $(ALL_OBJ) -o $(TEST_BIN) $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

examples: $(EX1_BIN) $(EX2_BIN) $(EX3_BIN)

$(EX1_BIN): $(ALL_OBJ) $(EX1_SRC)
	$(CC) $(CFLAGS) $(EX1_SRC) $(ALL_OBJ) -o $(EX1_BIN) $(LDFLAGS)

$(EX2_BIN): $(ALL_OBJ) $(EX2_SRC)
	$(CC) $(CFLAGS) $(EX2_SRC) $(ALL_OBJ) -o $(EX2_BIN) $(LDFLAGS)

$(EX3_BIN): $(ALL_OBJ) $(EX3_SRC)
	$(CC) $(CFLAGS) $(EX3_SRC) $(ALL_OBJ) -o $(EX3_BIN) $(LDFLAGS)

bench: $(ALL_OBJ) $(BENCH_SRC)
	$(CC) $(CFLAGS) $(BENCH_SRC) $(ALL_OBJ) -o $(BENCH_BIN) $(LDFLAGS)

demo: $(ALL_OBJ) $(DEMO_SRC)
	$(CC) $(CFLAGS) $(DEMO_SRC) $(ALL_OBJ) -o $(DEMO_BIN) $(LDFLAGS)

clean:
	rm -f $(ALL_OBJ) $(TEST_BIN) $(EX1_BIN) $(EX2_BIN) $(EX3_BIN) $(BENCH_BIN) $(DEMO_BIN)
	rm -f src/*.o
