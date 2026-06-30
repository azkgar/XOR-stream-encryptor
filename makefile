# Compiler and compilation flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LIBS = -pthread

# Target binary name
TARGET = encryptUtil

# Source and Object files discovery
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

# Default rule to build the application
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile source files to object files
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean