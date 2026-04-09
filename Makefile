# ─── Member 1 Makefile ──────────────────────────────────────────────────────
# Builds opal_core.c with the POSIX RAL and mock transport for desktop testing.
# No FreeRTOS or embedded toolchain required.
#
# Usage:
#   make          → build test binary
#   make test     → build + run all unit tests
#   make clean    → remove build artifacts
#   make check    → syntax-check all Member 1 headers

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c99 -g \
           -DOPAL_LOG_LEVEL=3 \
           -I.
LDFLAGS = -lpthread

SRC_CORE  = core/opal_core.c
SRC_RAL   = ral/opal_ral_posix.c
SRC_MOCK  = transport/opal_transport_mock.c
SRC_TEST  = tests/test_opal_core.c

BIN_TEST  = tests/test_opal_core

.PHONY: all test clean check

all: $(BIN_TEST)

$(BIN_TEST): $(SRC_TEST) $(SRC_CORE) $(SRC_RAL) $(SRC_MOCK)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo ""
	@echo "  Build OK →  $(BIN_TEST)"
	@echo "  Run with:   make test"
	@echo ""

test: $(BIN_TEST)
	@echo ""
	./$(BIN_TEST)
	@echo ""

check:
	@echo "Checking headers for Linux dependencies..."
	@grep -rn "^#include <linux/" core/ && \
		echo "ERROR: Linux headers found in core!" && exit 1 || \
		echo "  OK: no Linux headers in core/"
	@grep -rn "^#include <freertos\|^#include <FreeRTOS" core/ && \
		echo "ERROR: FreeRTOS headers found in core!" && exit 1 || \
		echo "  OK: no FreeRTOS headers in core/"
	@echo "  Portability check PASSED"

clean:
	rm -f $(BIN_TEST) *.o core/*.o ral/*.o transport/*.o tests/*.o
