# Near-Field Probe Diagnostic — Makefile
# Builds library, tests, and examples

CC = gcc
CFLAGS = -std=c11 -D_USE_MATH_DEFINES -Wall -Wextra -O2 -g
LDFLAGS = -lm

SRCDIR = src
INCDIR = include
TESTDIR = tests
EXDIR = examples
BUILDDIR = build

SRCS = $(SRCDIR)/nf_probe_core.c \
       $(SRCDIR)/nf_field_theory.c \
       $(SRCDIR)/nf_probe_calibration.c \
       $(SRCDIR)/nf_scanning.c \
       $(SRCDIR)/nf_signal_processing.c \
       $(SRCDIR)/nf_diagnostic.c

OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))
DEPS = $(wildcard $(INCDIR)/*.h)

TEST_SRC = $(TESTDIR)/test_nf_probe.c
TEST_BIN = $(BUILDDIR)/test_nf_probe

EX1_SRC = $(EXDIR)/ex1_microstrip_scan.c
EX1_BIN = $(BUILDDIR)/ex1_microstrip_scan
EX2_SRC = $(EXDIR)/ex2_ic_hotspot.c
EX2_BIN = $(BUILDDIR)/ex2_ic_hotspot
EX3_SRC = $(EXDIR)/ex3_pcb_diagnostic.c
EX3_BIN = $(BUILDDIR)/ex3_pcb_diagnostic

EXAMPLES = $(EX1_BIN) $(EX2_BIN) $(EX3_BIN)

.PHONY: all clean test examples

all: test examples

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(DEPS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(TEST_BIN): $(OBJS) $(TEST_SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) $(TEST_SRC) $(OBJS) $(LDFLAGS) -o $@

$(EX1_BIN): $(OBJS) $(EX1_SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) $(EX1_SRC) $(OBJS) $(LDFLAGS) -o $@

$(EX2_BIN): $(OBJS) $(EX2_SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) $(EX2_SRC) $(OBJS) $(LDFLAGS) -o $@

$(EX3_BIN): $(OBJS) $(EX3_SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) $(EX3_SRC) $(OBJS) $(LDFLAGS) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

examples: $(EXAMPLES)
	@echo "Examples built: $(EXAMPLES)"
	@echo "Run with:"
	@echo "  ./$(EX1_BIN)"
	@echo "  ./$(EX2_BIN)"
	@echo "  ./$(EX3_BIN)"

clean:
	rm -rf $(BUILDDIR) *.csv
	find . -name "*.o" -delete
