#!/bin/bash
#######################################################################################################
# @file run_tests.sh
# @brief Test suite for encryptUtil — XOR Stream Encryption Utility
#
# Usage:
#   cd <project_root>
#   bash tst/run_tests.sh
#
# The script creates temporary fixture files, runs all test cases, prints a detailed
# report to stdout, and exits with code 0 if all tests pass or 1 if any fail.
#
# @author Azkary Garcia
# @date June 30th, 2026
#######################################################################################################

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
BINARY="./encryptUtil"
TMP="/tmp/encryptUtil_test"
PASS=0
FAIL=0

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
run_test() {
    local id="$1"
    local desc="$2"
    local check="$3"
    local result
    result=$(eval "$check" 2>/dev/null)
    if [ "$result" = "PASS" ]; then
        printf "  [PASS] %-10s %s\n" "$id" "$desc"
        PASS=$((PASS + 1))
    else
        printf "  [FAIL] %-10s %s\n" "$id" "$desc"
        FAIL=$((FAIL + 1))
    fi
}

# ---------------------------------------------------------------------------
# Verify binary exists
# ---------------------------------------------------------------------------
if [ ! -x "$BINARY" ]; then
    echo "ERROR: $BINARY not found. Run 'make' first."
    exit 1
fi

# ---------------------------------------------------------------------------
# Create test fixtures
# ---------------------------------------------------------------------------
mkdir -p "$TMP"

# Key files
python3 -c "
open('$TMP/key_1byte.bin',  'wb').write(bytes([0x0F]))
open('$TMP/key_4byte.bin',  'wb').write(bytes([0xDE, 0xAD, 0xBE, 0xEF]))
open('$TMP/key_16byte.bin', 'wb').write(bytes([
    0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
    0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10]))
open('$TMP/key_empty.bin',  'wb').write(b'')
"

# Plaintext fixtures
python3 -c "
import os
open('$TMP/plain_ABC.bin',     'wb').write(b'ABC')
open('$TMP/plain_1byte.bin',   'wb').write(bytes([0x42]))
open('$TMP/plain_partial.bin', 'wb').write(bytes([0x01, 0x02]))       # 2 bytes, key is 4
open('$TMP/plain_exact.bin',   'wb').write(bytes([0xAA] * 4))         # exactly 1 block
open('$TMP/plain_multi.bin',   'wb').write(bytes([0xAA] * 10))        # 2.5 blocks (4-byte key)
open('$TMP/plain_binary.bin',  'wb').write(bytes(range(256)) * 4)     # 1024 bytes, all byte values
open('$TMP/plain_empty.bin',   'wb').write(b'')
open('$TMP/plain_1mb.bin',     'wb').write(os.urandom(1_000_000))     # 1MB random binary
"

# ---------------------------------------------------------------------------
# Test Report Header
# ---------------------------------------------------------------------------
echo ""
echo "========================================================================"
echo " encryptUtil — Test Report"
echo " Binary : $BINARY"
echo " Date   : $(date)"
echo "========================================================================"

# -----------------------------------------------------------------------
# Section 1: CLI / Error Handling
# -----------------------------------------------------------------------
echo ""
echo "── 1. CLI / Error Handling ─────────────────────────────────────────────"

run_test "CLI-01" "No arguments exits non-zero" \
    "timeout 3 $BINARY; [ \$? -ne 0 ] && echo PASS || echo FAIL"

run_test "CLI-02" "Missing -k flag exits non-zero" \
    "timeout 3 $BINARY -n 2; [ \$? -ne 0 ] && echo PASS || echo FAIL"

run_test "CLI-03" "Nonexistent key file exits non-zero" \
    "timeout 3 bash -c \"echo | $BINARY -n 1 -k /tmp/no_such_file.bin\"; [ \$? -ne 0 ] && echo PASS || echo FAIL"

run_test "CLI-04" "Empty key file exits non-zero" \
    "timeout 3 bash -c \"echo | $BINARY -n 1 -k $TMP/key_empty.bin\"; [ \$? -ne 0 ] && echo PASS || echo FAIL"

run_test "CLI-05" "Thread count 0 exits non-zero" \
    "timeout 3 bash -c \"echo | $BINARY -n 0 -k $TMP/key_1byte.bin\"; [ \$? -ne 0 ] && echo PASS || echo FAIL"

# -----------------------------------------------------------------------
# Section 2: Correctness — Known Values
# -----------------------------------------------------------------------
echo ""
echo "── 2. Correctness: Known Values ────────────────────────────────────────"
echo "   (hand-computed: key=0x0F, 'ABC' -> 0x4E 0x5C 0x7F)"

run_test "COR-01" "1-byte key, 'ABC' -> expected bytes 4e 5c 7f" \
    "out=\$(timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_1byte.bin < $TMP/plain_ABC.bin | od -An -tx1 | tr -d ' \n'\"); [ \"\$out\" = '4e5c7f' ] && echo PASS || echo FAIL"

run_test "COR-02" "Partial block (2 bytes, 4-byte key) -> 2 bytes output" \
    "b=\$(timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_4byte.bin < $TMP/plain_partial.bin | wc -c\"); [ \"\$b\" -eq 2 ] && echo PASS || echo FAIL"

run_test "COR-03" "Exact block (4 bytes, 4-byte key) -> 4 bytes output" \
    "b=\$(timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_4byte.bin < $TMP/plain_exact.bin | wc -c\"); [ \"\$b\" -eq 4 ] && echo PASS || echo FAIL"

run_test "COR-04" "Multi-block (10 bytes, 4-byte key) -> 10 bytes output" \
    "b=\$(timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_4byte.bin < $TMP/plain_multi.bin | wc -c\"); [ \"\$b\" -eq 10 ] && echo PASS || echo FAIL"

run_test "COR-05" "Binary input (all 256 byte values x4) -> 1024 bytes output" \
    "b=\$(timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_4byte.bin < $TMP/plain_binary.bin | wc -c\"); [ \"\$b\" -eq 1024 ] && echo PASS || echo FAIL"

# -----------------------------------------------------------------------
# Section 3: Round-trip (XOR self-inverse)
# -----------------------------------------------------------------------
echo ""
echo "── 3. Round-trip: encrypt(encrypt(x)) == x ─────────────────────────────"

for key in key_1byte key_4byte key_16byte; do
    run_test "RT-${key}" "Round-trip with $key (1MB input, n=4)" \
        "timeout 15 bash -c \"$BINARY -n 4 -k $TMP/${key}.bin < $TMP/plain_1mb.bin | $BINARY -n 4 -k $TMP/${key}.bin > $TMP/rt_${key}.bin\" && cmp $TMP/plain_1mb.bin $TMP/rt_${key}.bin && echo PASS || echo FAIL"
done

# -----------------------------------------------------------------------
# Section 4: Edge Cases
# -----------------------------------------------------------------------
echo ""
echo "── 4. Edge Cases ───────────────────────────────────────────────────────"

run_test "EDGE-01" "Empty stdin -> zero bytes output, clean exit" \
    "b=\$(timeout 5 bash -c \"$BINARY -n 4 -k $TMP/key_4byte.bin < $TMP/plain_empty.bin | wc -c\"); [ \"\$b\" -eq 0 ] && echo PASS || echo FAIL"

run_test "EDGE-02" "Single byte input -> single byte output" \
    "b=\$(timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_4byte.bin < $TMP/plain_1byte.bin | wc -c\"); [ \"\$b\" -eq 1 ] && echo PASS || echo FAIL"

run_test "EDGE-03" "1-byte key, 1MB input -> rotation period = 8 blocks, correct length" \
    "b=\$(timeout 15 bash -c \"$BINARY -n 4 -k $TMP/key_1byte.bin < $TMP/plain_1mb.bin | wc -c\"); [ \"\$b\" -eq 1000000 ] && echo PASS || echo FAIL"

run_test "EDGE-04" "16-byte key multi-byte rotation: round-trip on binary input" \
    "timeout 10 bash -c \"$BINARY -n 4 -k $TMP/key_16byte.bin < $TMP/plain_binary.bin | $BINARY -n 4 -k $TMP/key_16byte.bin > $TMP/rt_16.bin\" && cmp $TMP/plain_binary.bin $TMP/rt_16.bin && echo PASS || echo FAIL"

# -----------------------------------------------------------------------
# Section 5: Multi-thread Determinism
# -----------------------------------------------------------------------
echo ""
echo "── 5. Multi-thread Determinism: all thread counts produce same output ───"

for n in 1 2 4 8 16; do
    timeout 15 bash -c "$BINARY -n $n -k $TMP/key_4byte.bin < $TMP/plain_1mb.bin > $TMP/det_n${n}.bin 2>/dev/null"
done

for n in 2 4 8 16; do
    run_test "DET-n${n}" "n=1 and n=$n produce byte-identical output (1MB, 4-byte key)" \
        "cmp $TMP/det_n1.bin $TMP/det_n${n}.bin && echo PASS || echo FAIL"
done

# -----------------------------------------------------------------------
# Section 6: Stress / Large Input
# -----------------------------------------------------------------------
echo ""
echo "── 6. Stress / Large Input ─────────────────────────────────────────────"

for n in 1 8 16; do
    run_test "STR-n${n}" "1MB input, n=$n: correct byte count output" \
        "b=\$(timeout 15 bash -c \"$BINARY -n $n -k $TMP/key_4byte.bin < $TMP/plain_1mb.bin | wc -c\"); [ \"\$b\" -eq 1000000 ] && echo PASS || echo FAIL"
done

run_test "STR-RT" "1MB round-trip, n=8, 16-byte key" \
    "timeout 15 bash -c \"$BINARY -n 8 -k $TMP/key_16byte.bin < $TMP/plain_1mb.bin | $BINARY -n 8 -k $TMP/key_16byte.bin > $TMP/str_rt.bin\" && cmp $TMP/plain_1mb.bin $TMP/str_rt.bin && echo PASS || echo FAIL"

# -----------------------------------------------------------------------
# Cleanup
# -----------------------------------------------------------------------
rm -rf "$TMP"

# -----------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------
echo ""
echo "========================================================================"
printf " Results: %d passed, %d failed out of %d total\n" \
    $PASS $FAIL $((PASS + FAIL))
echo "========================================================================"
echo ""

[ $FAIL -eq 0 ] && exit 0 || exit 1
