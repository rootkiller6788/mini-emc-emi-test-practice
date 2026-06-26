# Makefile — mini-pre-compliance-low-cost-methods
# Builds library, tests, and examples for low-cost EMC pre-compliance methods.

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g -Iinclude
LDFLAGS  = -lm

LIB_SRCS = src/emi_precompliance_core.c \
           src/emi_probe_theory.c \
           src/emi_measurement_setup.c \
           src/emi_spectrum_analysis.c \
           src/emi_lowcost_methods.c \
           src/emi_correlation_uncertainty.c

LIB_OBJS = $(LIB_SRCS:.c=.o)

TEST_SRC   = tests/test_emi_precompliance.c
TEST_BIN   = tests/test_emi_precompliance

EX_SRC1 = examples/example_smps_prescan.c
EX_SRC2 = examples/example_arduino_emi_check.c
EX_SRC3 = examples/example_probe_calibration.c
EX_BIN1 = examples/example_smps_prescan
EX_BIN2 = examples/example_arduino_emi_check
EX_BIN3 = examples/example_probe_calibration

.PHONY: all clean test examples

all: test examples

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BIN): $(TEST_SRC) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(EX_BIN1): $(EX_SRC1) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(EX_BIN2): $(EX_SRC2) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(EX_BIN3): $(EX_SRC3) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

examples: $(EX_BIN1) $(EX_BIN2) $(EX_BIN3)

run-smps: $(EX_BIN1)
	./$(EX_BIN1)

run-arduino: $(EX_BIN2)
	./$(EX_BIN2)

run-probe: $(EX_BIN3)
	./$(EX_BIN3)

clean:
	rm -f src/*.o
	rm -f $(TEST_BIN)
	rm -f $(EX_BIN1) $(EX_BIN2) $(EX_BIN3)
