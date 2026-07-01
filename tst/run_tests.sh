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

# ---------------------------------------------------------------------------
# Create test fixtures
# ---------------------------------------------------------------------------
mkdir -p "$TMP"

# Restore permissions on any leftover noperm file from a previous interrupted
# run — otherwise Python or rm may fail on the next setup
chmod 644 "$TMP/key_noperm.bin" 2>/dev/null || true

# Key files
python3 -c "
open('$TMP/key_1byte.bin',  'wb').write(bytes([0x0F]))
open('$TMP/key_3byte.bin',  'wb').write(bytes([0xA1, 0xB2, 0xC3]))
open('$TMP/key_4byte.bin',  'wb').write(bytes([0xDE, 0xAD, 0xBE, 0xEF]))
open('$TMP/key_7byte.bin',  'wb').write(bytes([0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77]))
open('$TMP/key_16byte.bin', 'wb').write(bytes([
    0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
    0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10]))
open('$TMP/key_large.bin',  'wb').write(bytes(range(256)))
open('$TMP/key_empty.bin',  'wb').write(b'')
"

# Permission-denied key file — created in bash so chmod lifecycle stays
# entirely in shell, avoiding Python PermissionError on repeated runs
printf '\xAB\xCD' > "$TMP/key_noperm.bin"
chmod 000 "$TMP/key_noperm.bin"

# Plaintext fixtures
python3 -c "
import os
open('$TMP/plain_ABC.bin',     'wb').write(b'ABC')
open('$TMP/plain_1byte.bin',   'wb').write(bytes([0x42]))
open('$TMP/plain_partial.bin', 'wb').write(bytes([0x01, 0x02]))
open('$TMP/plain_exact.bin',   'wb').write(bytes([0xAA] * 4))
open('$TMP/plain_multi.bin',   'wb').write(bytes([0xAA] * 10))
open('$TMP/plain_binary.bin',  'wb').write(bytes(range(256)) * 4)
open('$TMP/plain_empty.bin',   'wb').write(b'')
open('$TMP/plain_3block.bin',  'wb').write(bytes([0xBB] * 12))
open('$TMP/plain_large.bin',   'wb').write(os.urandom(256_000))
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

run_test "CLI-06" "n=1 accepted as valid baseline (exits zero, produces output)" \
    "b=\$(_timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_1byte.bin < $TMP/plain_ABC.bin | wc -c\"); [ \"\$b\" -eq 3 ] && echo PASS || echo FAIL"

run_test "CLI-07" "Non-numeric -n value (-n abc) exits non-zero" \
    "_timeout 5 bash -c \"echo | $BINARY -n abc -k $TMP/key_1byte.bin\"; [ \$? -ne 0 ] && echo PASS || echo FAIL"

run_test "CLI-08" "Negative -n value (-n -1) exits non-zero" \
    "_timeout 5 bash -c \"echo | $BINARY -n -1 -k $TMP/key_1byte.bin\"; [ \$? -ne 0 ] && echo PASS || echo FAIL"

run_test "CLI-09" "-n flag with no following value exits non-zero" \
    "_timeout 5 bash -c \"echo | $BINARY -n -k $TMP/key_1byte.bin\"; [ \$? -ne 0 ] && echo PASS || echo FAIL"

run_test "CLI-10" "Unreadable key file exits non-zero (permission denied)" \
    "_timeout 5 bash -c \"echo | $BINARY -n 1 -k $TMP/key_noperm.bin\"; [ \$? -ne 0 ] && echo PASS || echo FAIL"

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

run_test "COR-06" "Large key (256 bytes) vs small input (3 bytes) -> 3 bytes output" \
    "b=\$(_timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_large.bin < $TMP/plain_ABC.bin | wc -c\"); [ \"\$b\" -eq 3 ] && echo PASS || echo FAIL"

run_test "COR-07" "Large key round-trip: encrypt(encrypt(x)) == x" \
    "_timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_large.bin < $TMP/plain_ABC.bin | $BINARY -n 1 -k $TMP/key_large.bin > $TMP/rt_largekey.bin\" && cmp $TMP/plain_ABC.bin $TMP/rt_largekey.bin && echo PASS || echo FAIL"

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

run_test "EDGE-05" "3-byte key: output size preserved (non-power-of-two rotation)" \
    "b=\$(_timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_3byte.bin < $TMP/plain_binary.bin | wc -c\"); [ \"\$b\" -eq 1024 ] && echo PASS || echo FAIL"

run_test "EDGE-06" "3-byte key: round-trip correctness" \
    "_timeout 5 bash -c \"$BINARY -n 4 -k $TMP/key_3byte.bin < $TMP/plain_binary.bin | $BINARY -n 4 -k $TMP/key_3byte.bin > $TMP/rt_3byte.bin\" && cmp $TMP/plain_binary.bin $TMP/rt_3byte.bin && echo PASS || echo FAIL"

run_test "EDGE-07" "7-byte key: output size preserved (non-power-of-two rotation)" \
    "b=\$(_timeout 5 bash -c \"$BINARY -n 1 -k $TMP/key_7byte.bin < $TMP/plain_binary.bin | wc -c\"); [ \"\$b\" -eq 1024 ] && echo PASS || echo FAIL"

run_test "EDGE-08" "7-byte key: round-trip correctness" \
    "_timeout 5 bash -c \"$BINARY -n 4 -k $TMP/key_7byte.bin < $TMP/plain_binary.bin | $BINARY -n 4 -k $TMP/key_7byte.bin > $TMP/rt_7byte.bin\" && cmp $TMP/plain_binary.bin $TMP/rt_7byte.bin && echo PASS || echo FAIL"

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

# ── 6. Thread Starvation: N > Block Count ────────────────────────────
echo ""
echo "── 6. Thread Starvation: N > Block Count ───────────────────────────────"
# plain_3block.bin is exactly 3 x 4-byte blocks; spawning 16 workers means 13 will
# find an empty queue and must exit cleanly without deadlocking or hanging.

run_test "STARV-01" "n=16 threads, 3-block input: no deadlock (exits within timeout)" \
    "_timeout 5 bash -c \"$BINARY -n 16 -k $TMP/key_4byte.bin < $TMP/plain_3block.bin > $TMP/starv_out.bin\"; [ \$? -eq 0 ] && echo PASS || echo FAIL"

run_test "STARV-02" "n=16 threads, 3-block input: byte count preserved" \
    "b=\$(wc -c < $TMP/starv_out.bin 2>/dev/null || echo -1); [ \"\$b\" -eq 12 ] && echo PASS || echo FAIL"

run_test "STARV-03" "n=16 threads, 3-block input: round-trip correctness" \
    "_timeout 5 bash -c \"$BINARY -n 16 -k $TMP/key_4byte.bin < $TMP/plain_3block.bin | $BINARY -n 16 -k $TMP/key_4byte.bin > $TMP/starv_rt.bin\" && cmp $TMP/plain_3block.bin $TMP/starv_rt.bin && echo PASS || echo FAIL"

run_test "STARV-04" "n=16 threads, single-byte input: no deadlock, 1 byte output" \
    "b=\$(_timeout 5 bash -c \"$BINARY -n 16 -k $TMP/key_4byte.bin < $TMP/plain_1byte.bin | wc -c\"); [ \"\$b\" -eq 1 ] && echo PASS || echo FAIL"

# ── 7. Stress / Large Input ─────────────────────────────────────────────
echo ""
echo "── 7. Stress / Large Input ─────────────────────────────────────────────"

for n in 1 8 16; do
    run_test "STR-n${n}" "Large input, n=$n: correct byte count" \
        "b=\$(_timeout 10 bash -c \"$BINARY -n $n -k $TMP/key_4byte.bin < $TMP/plain_large.bin | wc -c\"); [ \"\$b\" -eq 256000 ] && echo PASS || echo FAIL"
done

run_test "STR-RT" "Large round-trip, n=8, 16-byte key" \
    "_timeout 10 bash -c \"$BINARY -n 8 -k $TMP/key_16byte.bin < $TMP/plain_large.bin | $BINARY -n 8 -k $TMP/key_16byte.bin > $TMP/str_rt.bin\" && cmp $TMP/plain_large.bin $TMP/str_rt.bin && echo PASS || echo FAIL"

# ── Cleanup & Summary ───────────────────────────────────────────────────
chmod 644 "$TMP/key_noperm.bin" 2>/dev/null || true   # restore before rm to avoid rm errors
rm -rf "$TMP"
echo ""
echo "========================================================================"
printf " Results: %d passed, %d failed out of %d total\n" $PASS $FAIL $((PASS + FAIL))
echo "========================================================================"
echo ""

[ $FAIL -eq 0 ] && exit 0 || exit 1