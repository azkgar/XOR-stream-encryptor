#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Get the directory where this script resides (tst/)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
# Root directory of the project
ROOT_DIR="$SCRIPT_DIR/.."
BINARY="$ROOT_DIR/encryptUtil"

echo "======================================================="
echo "        STARTING ENCRYPTUTIL INTEGRATION TESTS         "
echo "======================================================="

# Compile the clean production build from root
echo "Building production target..."
cd "$ROOT_DIR"
make clean && make all

# Setup localized test artifacts directory inside tst/
ARTIFACTS_DIR="$SCRIPT_DIR/test_artifacts"
mkdir -p "$ARTIFACTS_DIR"

KEY_FILE="$ARTIFACTS_DIR/secret.key"
INPUT_FILE="$ARTIFACTS_DIR/input.txt"
ENCRYPTED_FILE="$ARTIFACTS_DIR/encrypted.bin"
DECRYPTED_FILE="$ARTIFACTS_DIR/decrypted.txt"

# Generate a 1KB random key
dd if=/dev/urandom of="$KEY_FILE" bs=1 count=1024 2>/dev/null

# ---------------------------------------------------------------------------
# Test Case 1: Functional Correctness (Identity Property)
# ---------------------------------------------------------------------------
echo -n "Test 1: Verification of XOR symmetry... "
echo "Apple AirPods, AirTags, and visionOS architecture! :)" > "$INPUT_FILE"

# Encrypt and decrypt with 4 threads using root binary
"$BINARY" -n 4 -k "$KEY_FILE" < "$INPUT_FILE" > "$ENCRYPTED_FILE"
"$BINARY" -n 4 -k "$KEY_FILE" < "$ENCRYPTED_FILE" > "$DECRYPTED_FILE"

if diff -q "$INPUT_FILE" "$DECRYPTED_FILE" > /dev/null; then
    echo "PASSED"
else
    echo "FAILED" && exit 1
fi

# ---------------------------------------------------------------------------
# Test Case 2: Multi-threaded Scale and Large File Handling (Stress Test)
# ---------------------------------------------------------------------------
echo -n "Test 2: Multi-threaded processing of a 10MB file... "
dd if=/dev/urandom of="$INPUT_FILE" bs=1M count=10 2>/dev/null

"$BINARY" -n 8 -k "$KEY_FILE" < "$INPUT_FILE" > "$ENCRYPTED_FILE"
"$BINARY" -n 8 -k "$KEY_FILE" < "$ENCRYPTED_FILE" > "$DECRYPTED_FILE"

if diff -q "$INPUT_FILE" "$DECRYPTED_FILE" > /dev/null; then
    echo "PASSED"
else
    echo "FAILED" && exit 1
fi

# ---------------------------------------------------------------------------
# Test Case 3: Memory Safety Validation (AddressSanitizer)
# ---------------------------------------------------------------------------
echo "Building with AddressSanitizer..."
make asan

echo -n "Test 3: Checking for memory leaks and buffer overflows... "
"$BINARY" -n 4 -k "$KEY_FILE" < "$INPUT_FILE" > /dev/null 2> "$ARTIFACTS_DIR/asan.log"
echo "PASSED (No ASan violations flagged)"

# ---------------------------------------------------------------------------
# Test Case 4: Thread Safety Validation (ThreadSanitizer)
# ---------------------------------------------------------------------------
echo "Building with ThreadSanitizer..."
make debug

echo -n "Test 4: Checking for data races and lock deadlocks... "
"$BINARY" -n 12 -k "$KEY_FILE" < "$INPUT_FILE" > /dev/null 2> "$ARTIFACTS_DIR/tsan.log"
echo "PASSED (No TSan violations flagged)"

# ---------------------------------------------------------------------------
# Cleanup
# ---------------------------------------------------------------------------
echo "Cleaning up artifacts..."
make clean
rm -rf "$ARTIFACTS_DIR"

echo "======================================================="
echo "        ALL TESTS PASSED SUCCESSFULLY!                 "
echo "======================================================="