###############################################################################
# Makefile for encryptUtil — XOR Stream Encryption Utility
#
# Author: Azkary Garcia
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
# Sanitize / Debug Targets (Using recursive make to prevent race conditions)
# ---------------------------------------------------------------------------
# ThreadSanitizer for data races
debug: CFLAGS += -g -fsanitize=thread
debug: LDFLAGS += -fsanitize=thread
debug:
	$(MAKE) clean
	$(MAKE) $(TARGET)

# AddressSanitizer for memory leaks and buffer overflows
asan: CFLAGS += -g -fsanitize=address
asan: LDFLAGS += -fsanitize=address
asan:
	$(MAKE) clean
	$(MAKE) $(TARGET)

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
