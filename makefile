###############################################################################
# Makefile for encryptUtil — XOR Stream Encryption Utility
#
# Author: Azkary Garcia
# Date:   June 30th, 2026
###############################################################################

# ---------------------------------------------------------------------------
# Compiler and flags
# ---------------------------------------------------------------------------
# CC: which compiler to invoke
CC = gcc

# CFLAGS: flags passed during compilation (turning .c -> .o)
#   -Wall -Wextra : enable broad compiler warnings (catches subtle bugs like
#                   implicit conversions, unused variables, etc.)
#   -std=c99      : we rely on C99 features (mixed declarations/code, etc.)
#   -Iinclude     : tells the compiler to look in include/ for header files,
#                   so "#include "queue.h"" resolves regardless of which
#                   directory make is invoked from
CFLAGS = -Wall -Wextra -std=c99 -Iinclude

# LDFLAGS: flags passed during linking (combining .o files -> executable)
#   -lpthread : link against the POSIX threads library, required since we
#               use pthread_create, pthread_mutex_*, pthread_cond_*, etc.
LDFLAGS = -lpthread

# ---------------------------------------------------------------------------
# Source / object / target definitions
# ---------------------------------------------------------------------------
# SRC: every .c file that needs to be compiled
SRC = src/main.c src/crypto.c src/queue.c src/utils.c

# OBJ: derived automatically from SRC by swapping .c -> .o
#      (e.g. src/main.c -> src/main.o)
OBJ = $(SRC:.c=.o)

# TARGET: name of the final executable, matching the spec's usage string
#         (encryptUtil [-n #] [-k keyfile])
TARGET = encryptUtil

# ---------------------------------------------------------------------------
# Default target
# ---------------------------------------------------------------------------
# Running "make" with no arguments builds $(TARGET).
.DEFAULT_GOAL := all

all: $(TARGET)

# ---------------------------------------------------------------------------
# Link step: combine all object files into the final executable
# ---------------------------------------------------------------------------
# $(OBJ)    : prerequisites — this rule re-runs if any .o file changes
# $@        : automatic variable meaning "the target" (encryptUtil)
# $(LDFLAGS): linker flags placed after objects, which is the safest
#             ordering for -lpthread on most linkers
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# ---------------------------------------------------------------------------
# Compile step: pattern rule for turning any src/X.c into src/X.o
# ---------------------------------------------------------------------------
# $<: automatic variable meaning "the first prerequisite" (the .c file)
# $@: automatic variable meaning "the target" (the .o file)
# -c: compile only, don't link (produces an object file)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------
# Debug build: same as above, but with debug symbols and ThreadSanitizer
# ---------------------------------------------------------------------------
# -g              : include debug symbols (needed for gdb/lldb)
# -fsanitize=thread : ThreadSanitizer — instruments the binary to detect
#                     data races, lock ordering issues, and other threading
#                     bugs at runtime. Extremely valuable for this project
#                     given how much correctness depends on synchronization.
# Note: ThreadSanitizer requires the SAME flag at both compile and link time,
#       so we override CFLAGS/LDFLAGS for this target rather than reusing
#       the object files built by "all" (which lack -fsanitize=thread).
debug: CFLAGS += -g -fsanitize=thread
debug: LDFLAGS += -fsanitize=thread
debug: clean $(TARGET)

# ---------------------------------------------------------------------------
# Clean target: remove all build artifacts
# ---------------------------------------------------------------------------
# .PHONY tells make that "clean" (and "all", "debug") are not real files,
# so make won't get confused if a file named "clean" ever exists in the
# directory.
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all debug clean
