###############################################################################
# Makefile for encryptUtil — XOR Stream Encryption Utility
#
# Author: Azkary Garcia using Claude.ai
# Date:   June 30th, 2026
###############################################################################

# ---------------------------------------------------------------------------
# Compiler and flags
# ---------------------------------------------------------------------------
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Iinclude
LDFLAGS = -lpthread

# ---------------------------------------------------------------------------
# Source / object / target definitions
# ---------------------------------------------------------------------------
SRC = src/main.c src/crypto.c src/queue.c src/utils.c
OBJ = $(SRC:.c=.o)
TARGET = encryptUtil

# Collect all headers to track dependencies accurately
HEADERS = $(wildcard include/*.h)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.DEFAULT_GOAL := all

all: $(TARGET)

# Link step
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# Compile step
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------
# Sanitize / Debug Targets (Using explicit flag injection for sub-makes)
# ---------------------------------------------------------------------------
# ThreadSanitizer for data races
debug:
	$(MAKE) clean
	$(MAKE) $(TARGET) CFLAGS="$(CFLAGS) -g -fsanitize=thread" LDFLAGS="$(LDFLAGS) -fsanitize=thread"

# AddressSanitizer for memory leaks and buffer overflows
asan:
	$(MAKE) clean
	$(MAKE) $(TARGET) CFLAGS="$(CFLAGS) -g -fsanitize=address" LDFLAGS="$(LDFLAGS) -fsanitize=address"

# ---------------------------------------------------------------------------
# Clean target
# ---------------------------------------------------------------------------
clean:
	rm -f $(OBJ) $(TARGET)

# ---------------------------------------------------------------------------
# Test target: execute the automated integration suite
# ---------------------------------------------------------------------------
test: all
	@chmod +x tst/run_tests.sh
	@./tst/run_tests.sh

.PHONY: all debug asan clean
