#!/bin/sh
#
# POSIX test suite for approx
#

APPROX="./approx"
PASSED=0
FAILED=0

run_test() {
	desc="$1"
	cmd="$2"
	expected_status="$3"
	expected_output="$4"

	actual_output=$(eval "$cmd" 2>/dev/null)
	status=$?

	if [ "$status" -ne "$expected_status" ]; then
		printf "[FAIL] %s (expected exit status %d, got %d)\n" "$desc" "$expected_status" "$status"
		FAILED=$((FAILED + 1))
		return
	fi

	if [ -n "$expected_output" ] && [ "$actual_output" != "$expected_output" ]; then
		printf "[FAIL] %s\n  Expected:\n%s\n  Got:\n%s\n" "$desc" "$expected_output" "$actual_output"
		FAILED=$((FAILED + 1))
		return
	fi

	printf "[PASS] %s\n" "$desc"
	PASSED=$((PASSED + 1))
}

printf "========================================\n"
printf "Running approx test suite\n"
printf "========================================\n"

# Test 1: Exact substring match
run_test "Exact substring match on stdin" \
	'printf "2026-08-18 [INFO] database connected\n2026-08-18 [ERROR] connection timeout\n" | '"$APPROX"' "connection timeout"' \
	0 \
	"2026-08-18 [ERROR] connection timeout"

# Test 2: Typo tolerance match
run_test "Typo tolerance fuzzy match" \
	'printf "warning: recieve buffer overflow\n" | '"$APPROX"' "receive buffer"' \
	0 \
	"warning: recieve buffer overflow"

# Test 3: No match exit code (should be 1)
run_test "No match returns exit code 1" \
	'printf "apple\nbanana\norange\n" | '"$APPROX"' "strawberry"' \
	1 \
	""

# Test 4: Threshold filtering
run_test "High threshold filters weak match" \
	'printf "app\n" | '"$APPROX"' -t 0.80 "apple"' \
	1 \
	""

run_test "Lower threshold accepts weak match" \
	'printf "app\n" | '"$APPROX"' -t 0.50 "apple"' \
	0 \
	"app"

# Test 5: Case insensitivity with -i
run_test "Case-sensitive fails on mismatched case" \
	'printf "FATAL ERROR\n" | '"$APPROX"' -t 1.0 "fatal error"' \
	1 \
	""

run_test "Case-insensitive succeeds with -i" \
	'printf "FATAL ERROR\n" | '"$APPROX"' -i -t 1.0 "fatal error"' \
	0 \
	"FATAL ERROR"

# Test 6: Invert match with -v
run_test "Invert match outputs non-matching lines" \
	'printf "good line\nbad line\ngood line 2\n" | '"$APPROX"' -v "good"' \
	0 \
	"bad line"

# Test 7: Score prefix with -s
run_test "Score prefix with -s outputs 1.00 on exact match" \
	'printf "target string\n" | '"$APPROX"' -s "target string"' \
	0 \
	"$(printf '1.00\ttarget string')"

# Test 8: Top-N ranking with -n
run_test "Top-N ranking orders by highest score" \
	'printf "unrelated text\nreceive\nrecieve\nrec\n" | '"$APPROX"' -n 2 "receive"' \
	0 \
	"$(printf 'receive\nrecieve')"

# Test 9: Exact full-line mode with -e
run_test "Default substring mode matches word in long line" \
	'printf "a very long log entry with secret token inside it\n" | '"$APPROX"' -t 0.90 "secret token"' \
	0 \
	"a very long log entry with secret token inside it"

run_test "Exact full-line mode (-e) fails word in long line" \
	'printf "a very long log entry with secret token inside it\n" | '"$APPROX"' -e -t 0.90 "secret token"' \
	1 \
	""

# Test 10: Multi-file input and stdin
tmp1="/tmp/approx_test_1.$$"
tmp2="/tmp/approx_test_2.$$"
printf "alpha connection\n" > "$tmp1"
printf "beta timeout\n" > "$tmp2"

run_test "Multiple file arguments" \
	''"$APPROX"' "connection" '"$tmp1"' '"$tmp2"'' \
	0 \
	"alpha connection"

run_test "Combined stdin and file arguments" \
	'printf "gamma connection\n" | '"$APPROX"' "connection" '"$tmp1"' -' \
	0 \
	"$(printf 'alpha connection\ngamma connection')"

rm -f "$tmp1" "$tmp2"

# Test 11: Error handling on invalid arguments (exit code 2)
run_test "Invalid threshold value returns exit code 2" \
	''"$APPROX"' -t 1.5 "test"' \
	2 \
	""

run_test "NaN threshold value returns exit code 2" \
	''"$APPROX"' -t NaN "test"' \
	2 \
	""

run_test "Invalid count value returns exit code 2" \
	''"$APPROX"' -n 0 "test"' \
	2 \
	""

run_test "Missing pattern returns exit code 2" \
	''"$APPROX"'' \
	2 \
	""

# Test 12: Version flag -V
run_test "Version flag outputs version and returns 0" \
	''"$APPROX"' -V' \
	0 \
	"approx-1.0"

# Test 13: Empty input handling
run_test "Empty input produces exit code 1" \
	'printf "" | '"$APPROX"' "something"' \
	1 \
	""

# Test 14: Very long line handling (10,000 chars)
run_test "Long line streaming memory test" \
	'awk '\''BEGIN { for (i=0; i<5000; i++) printf "x"; printf " needle "; for (i=0; i<5000; i++) printf "y"; printf "\n" }'\'' | '"$APPROX"' "needle"' \
	0 \
	"$(awk 'BEGIN { for (i=0; i<5000; i++) printf "x"; printf " needle "; for (i=0; i<5000; i++) printf "y"; printf "\n" }')"

# Test 15: UTF-8 / multi-byte safety (no crashes)
run_test "UTF-8 multi-byte string handling" \
	'printf "café au lait\npiñata party\n" | '"$APPROX"' "café"' \
	0 \
	"café au lait"

printf "========================================\n"
printf "Tests passed: %d, Failed: %d\n" "$PASSED" "$FAILED"
printf "========================================\n"

if [ "$FAILED" -ne 0 ]; then
	exit 1
fi
exit 0
