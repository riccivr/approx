#!/bin/sh
#
# Fuzzing, Stress, and Malformed Input Tests for approx
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
printf "Running Stress & Malformed Input Tests\n"
printf "========================================\n"

# Test 1: Missing trailing newline at EOF
run_test "Unterminated line without trailing newline" \
	'printf "unterminated exact match" | '"$APPROX"' "unterminated exact match"' \
	0 \
	"unterminated exact match"

# Test 2: Windows / DOS CRLF line endings
run_test "Windows CRLF (\\r\\n) line endings stripped correctly" \
	'printf "windows log entry\r\nsecond entry\r\n" | '"$APPROX"' -s "windows log"' \
	0 \
	"$(printf '1.00\twindows log entry')"

# Test 3: Classic MacOS isolated CR (\\r) line endings
run_test "Classic Mac CR (\\r) line ending stripped" \
	'printf "mac log entry\r" | '"$APPROX"' -s "mac log entry"' \
	0 \
	"$(printf '1.00\tmac log entry')"

# Test 4: Pattern longer than input line
run_test "Pattern much longer than input lines" \
	'printf "short\nline\ntest\n" | '"$APPROX"' "this_pattern_is_much_longer_than_any_line"' \
	1 \
	""

# Test 5: Empty line matching
run_test "Empty lines in stream do not crash" \
	'printf "\n\n\nmatch me\n\n" | '"$APPROX"' "match me"' \
	0 \
	"match me"

# Test 6: 500-character query pattern
long_pat=$(awk 'BEGIN { for (i=0; i<500; i++) printf "a" }')
run_test "500-character long query pattern match" \
	'printf "%s\n" "'"$long_pat"'" | '"$APPROX"' "'"$long_pat"'"' \
	0 \
	"$long_pat"

# Test 7: Binary stream and embedded NUL safety (no crashes/hangs)
run_test "Binary data /dev/urandom chunk does not crash" \
	'head -c 4096 /dev/urandom 2>/dev/null | '"$APPROX"' "some_pattern" >/dev/null 2>&1; echo $?' \
	0 \
	"1"

# Test 8: 50,000 continuous lines streaming (memory leak & throughput sanity)
run_test "50,000 streaming lines" \
	'awk '\''BEGIN { for (i=1; i<=50000; i++) { if (i==25000) print "TARGET_HIT"; else print "noise_line_" i } }'\'' | '"$APPROX"' "TARGET_HIT"' \
	0 \
	"TARGET_HIT"

# Test 9: Top-N over 20,000 lines
run_test "Top-3 ranking over 20,000 lines" \
	'awk '\''BEGIN { for (i=1; i<=20000; i++) print "line " i; print "super match 1"; print "super match 2"; print "super match 3" }'\'' | '"$APPROX"' -n 3 "super match"' \
	0 \
	"$(printf 'super match 1\nsuper match 2\nsuper match 3')"

printf "========================================\n"
printf "Stress & Robustness: %d passed, %d failed\n" "$PASSED" "$FAILED"
printf "========================================\n"

if [ "$FAILED" -ne 0 ]; then
	exit 1
fi
exit 0
