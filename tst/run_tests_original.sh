#!/bin/bash
#######################################################################################################
# @file run_tests.sh
# @brief Test suite for encryptUtil — XOR Stream Encryption Utility
#######################################################################################################

set -euo pipefail

if command -v timeout &>/dev/null; then
    _timeout() { timeout "$@"; }
elif command -v gtimeout &>/dev/null; then
    _timeout() { gtimeout "$@"; }
else
    _timeout() {
        local secs="$1"; shift
        "$@" &
        local pid=$!
        (sleep "$secs" && kill -TERM "$pid" 2>/dev/null) &
        local watchdog=$!
        wait "$pid" 2>/dev/null
        local rc=$?
        kill "$watchdog" 2>/dev/null
        wait "$watchdog" 2>/dev/null
        return $rc
    }
fi

BINARY="./encryptUtil"
TMP="/tmp/encryptUtil_test"
PASS=0
FAIL=0

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

if [ ! -x "$BINARY" ]; then
    echo "ERROR: $BINARY not found. Run 'make' first."
    exit 1
fi

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

# Plaintext fixtures (Optimized scale)
python3 -c "
import os
open('$TMP/plain_ABC.bin',     'wb').write(b'ABC')
open('$TMP/plain_1byte.bin',   'wb').write(bytes([0x42]))
open('$TMP/plain_partial.bin', 'wb').write(bytes([0x01, 0x02]))
open('$TMP/plain_exact.bin',   'wb').write(bytes([0xAA] * 4))
open('$TMP/plain_multi.bin',   'wb').write(bytes([0xAA] * 10))
open('$TMP/plain_binary.bin',  'wb').write(bytes(range(256)) * 4)
open('$TMP/plain_empty.bin',   'wb').write(b'')
open('$TMP/plain_large.bin',   'wb').write(os.urandom(256_000)) # Reduced to 256KB for performance
"

echo ""
echo "========================================================================"
echo " encryptUtil — Test Report"
echo "========================================================================"

# ── 1. CLI / Error Handling ─────────────────────────────────────────────
echo ""
echo "── 1. CLI / Error Handling ─────────────────────────────────────────────"

run_test "CLI-01" "No arguments exits non-zero" \
    "_timeout 5 $BINARY; [ \$? -ne 0 ] && echo PASS || echo FAIL"

run_test "CLI-02" "Missing -k flag exits non-zero" \
    "_timeout 5 $BINARY -n 2; [ \$? -ne 0 ] && echo PASS || echo FAIL"

run_test "CLI-03" "Nonexistent key file exits non-zero" \
    "_timeout 5 bash -c \"echo | $BINARY -n 1 -k /tmp/no_such_file.bin\"; [ \$? -ne 0 ] && echo PASS || echo FAIL"

run_test "CLI-04" "Empty key file exits non-zero" \
    "_timeout 5 bash -c \"echo | $BINARY -n 1 -k $TMP/key_empty.bin\"; [ \$? -ne 0 ] && echo PASS || echo FAIL"

run_test "CLI-05" "Thread count 0 exits non-zero" \
    "_timeout 5 bash -c \"echo | $BINARY -n 0 -k $TMP/key_1byte.bin\"; [ \$? -ne 0 ] && echo PASS || echo FAIL"

# ── 2. Correctness: Known Values ────────────────────────────────────────
echo ""
echo "── 2. Correctness: Known Values ────────────────────────────────────────"

run_test "COR-01" "1-byte key, 'ABC' -> expected bytes 4e 5c 7f" \
    "out=\$(_timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_1byte.bin < $TMP/plain_ABC.bin | od -An -tx1 | tr -d ' \n'\"); [ \"\$out\" = '4e5c7f' ] && echo PASS || echo FAIL"

run_test "COR-02" "Partial block (2 bytes, 4-byte key) -> 2 bytes output" \
    "b=\$(_timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_4byte.bin < $TMP/plain_partial.bin | wc -c\"); [ \"\$b\" -eq 2 ] && echo PASS || echo FAIL"

run_test "COR-03" "Exact block (4 bytes, 4-byte key) -> 4 bytes output" \
    "b=\$(_timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_4byte.bin < $TMP/plain_exact.bin | wc -c\"); [ \"\$b\" -eq 4 ] && echo PASS || echo FAIL"

run_test "COR-04" "Multi-block (10 bytes, 4-byte key) -> 10 bytes output" \
    "b=\$(_timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_4byte.bin < $TMP/plain_multi.bin | wc -c\"); [ \"\$b\" -eq 10 ] && echo PASS || echo FAIL"

run_test "COR-05" "Binary input -> 1024 bytes output" \
    "b=\$(_timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_4byte.bin < $TMP/plain_binary.bin | wc -c\"); [ \"\$b\" -eq 1024 ] && echo PASS || echo FAIL"

# ── 3. Round-trip ───────────────────────────────────────────────────────
echo ""
echo "── 3. Round-trip: encrypt(encrypt(x)) == x ─────────────────────────────"

for key in key_1byte key_4byte key_16byte; do
    run_test "RT-${key}" "Round-trip with $key (n=4)" \
        "_timeout 10 bash -c \"$BINARY -n 4 -k $TMP/${key}.bin < $TMP/plain_large.bin | $BINARY -n 4 -k $TMP/${key}.bin > $TMP/rt_${key}.bin\" && cmp $TMP/plain_large.bin $TMP/rt_${key}.bin && echo PASS || echo FAIL"
done

# ── 4. Edge Cases ───────────────────────────────────────────────────────
echo ""
echo "── 4. Edge Cases ───────────────────────────────────────────────────────"

run_test "EDGE-01" "Empty stdin -> zero bytes output" \
    "b=\$(_timeout 5 bash -c \"$BINARY -n 4 -k $TMP/key_4byte.bin < $TMP/plain_empty.bin | wc -c\"); [ \"\$b\" -eq 0 ] && echo PASS || echo FAIL"

run_test "EDGE-02" "Single byte input -> single byte output" \
    "b=\$(_timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_4byte.bin < $TMP/plain_1byte.bin | wc -c\"); [ \"\$b\" -eq 1 ] && echo PASS || echo FAIL"

run_test "EDGE-03" "Correct large byte count constraint" \
    "b=\$(_timeout 10 bash -c \"$BINARY -n 4 -k $TMP/key_1byte.bin < $TMP/plain_large.bin | wc -c\"); [ \"\$b\" -eq 256000 ] && echo PASS || echo FAIL"

run_test "EDGE-04" "16-byte key multi-byte rotation" \
    "_timeout 5 bash -c \"$BINARY -n 4 -k $TMP/key_16byte.bin < $TMP/plain_binary.bin | $BINARY -n 4 -k $TMP/key_16byte.bin > $TMP/rt_16.bin\" && cmp $TMP/plain_binary.bin $TMP/rt_16.bin && echo PASS || echo FAIL"

# ── 5. Multi-thread Determinism ───────────────────────────────────────
echo ""
echo "── 5. Multi-thread Determinism ─────────────────────────────────────────"

# Execute safely using a tightened timeout per run to catch deadlocks instantly
for n in 1 2 4 8 16; do
    _timeout 5 bash -c "$BINARY -n $n -k $TMP/key_4byte.bin < $TMP/plain_large.bin > $TMP/det_n${n}.bin 2>/dev/null" || touch "$TMP/timeout_failed"
done

if [ -f "$TMP/timeout_failed" ]; then
    echo "  [FAIL] DET-Loop   One or more background thread variations deadlocked/timed out."
    FAIL=$((FAIL + 1))
else
    for n in 2 4 8 16; do
        run_test "DET-n${n}" "n=1 and n=$n produce byte-identical output" \
            "cmp $TMP/det_n1.bin $TMP/det_n${n}.bin && echo PASS || echo FAIL"
    done
fi

# ── 6. Stress / Large Input ─────────────────────────────────────────────
echo ""
echo "── 6. Stress / Large Input ─────────────────────────────────────────────"

for n in 1 8 16; do
    run_test "STR-n${n}" "Large input, n=$n: correct byte count" \
        "b=\$(_timeout 10 bash -c \"$BINARY -n $n -k $TMP/key_4byte.bin < $TMP/plain_large.bin | wc -c\"); [ \"\$b\" -eq 256000 ] && echo PASS || echo FAIL"
done

run_test "STR-RT" "Large round-trip, n=8, 16-byte key" \
    "_timeout 10 bash -c \"$BINARY -n 8 -k $TMP/key_16byte.bin < $TMP/plain_large.bin | $BINARY -n 8 -k $TMP/key_16byte.bin > $TMP/str_rt.bin\" && cmp $TMP/plain_large.bin $TMP/str_rt.bin && echo PASS || echo FAIL"

# ── Cleanup & Summary ───────────────────────────────────────────────────
rm -rf "$TMP"
echo ""
echo "========================================================================"
printf " Results: %d passed, %d failed out of %d total\n" $PASS $FAIL $((PASS + FAIL))
echo "========================================================================"
echo ""

[ $FAIL -eq 0 ] && exit 0 || exit 1