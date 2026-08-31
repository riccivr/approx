#!/bin/sh
#
# POSIX Utility Syntax Conformance Tests (IEEE Std 1003.1 Guidelines 1-14)
#

APPROX="./approx"
PASSED=0
FAILED=0

tmp1="posix_test_1.$$.tmp"
tmp2="posix_test_2.$$.tmp"
trap 'rm -f "$tmp1" "$tmp2"' EXIT INT TERM

run_test() {
	desc="$1"
	cmd="$2"
	expected_status="$3"
	expected_output="$4"

	actual_output=$(eval "$cmd" 2>/dev/null)
	status=$?
	actual_output=$(printf '%s' "$actual_output" | tr -d '\r')
	expected_output=$(printf '%s' "$expected_output" | tr -d '\r')

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
printf "Running POSIX Conformance Tests\n"
printf "========================================\n"

# Guideline 3, 4, 5: Combined single-letter flags
run_test "Grouped flags (-is)" \
	'printf "HELLO WORLD\n" | '"$APPROX"' -is "hello world"' \
	0 \
	"$(printf '1.00\tHELLO WORLD')"

run_test "Grouped flags with invert (-iv)" \
	'printf "HELLO WORLD\ngoodbye\n" | '"$APPROX"' -iv "hello"' \
	0 \
	"goodbye"

# Guideline 5, 6, 7: Attached vs detached option-arguments
run_test "Attached option-argument for -t (-t0.85)" \
	'printf "exact match\n" | '"$APPROX"' -t0.85 "exact match"' \
	0 \
	"exact match"

run_test "Attached option-argument for -n (-n2)" \
	'printf "first match\nsecond match\nthird match\n" | '"$APPROX"' -n2 "match"' \
	0 \
	"$(printf 'first match\nsecond match')"

run_test "Attached option-argument combined with flags (-ist0.85)" \
	'printf "EXACT MATCH\n" | '"$APPROX"' -ist0.85 "exact match"' \
	0 \
	"$(printf '1.00\tEXACT MATCH')"

# Guideline 10: End-of-options delimiter (--)
run_test "End of options (--) for pattern starting with dash" \
	'printf "%s\n%s\n" "-flag-like-text" "normal text" | '"$APPROX"' -- -flag' \
	0 \
	"-flag-like-text"

run_test "Flags before end-of-options (-- -flag)" \
	'printf "%s\n%s\n" "-FLAG-LIKE-TEXT" "normal text" | '"$APPROX"' -i -- -flag' \
	0 \
	"-FLAG-LIKE-TEXT"

# Guideline 13: '-' designates standard input
printf "file1 alpha\n" > "$tmp1"
printf "file2 beta\n" > "$tmp2"

run_test "Interleaved stdin '-' between files" \
	'printf "stdin gamma\n" | '"$APPROX"' "alpha" '"$tmp1"' - '"$tmp2"'' \
	0 \
	"$tmp1:file1 alpha"

run_test "Interleaved stdin '-' matching stdin line" \
	'printf "stdin alpha\n" | '"$APPROX"' "alpha" '"$tmp1"' - '"$tmp2"'' \
	0 \
	"$(printf '%s:file1 alpha\n(standard input):stdin alpha' "$tmp1")"

# Option argument validation & missing operands
run_test "Missing option argument for -t at end of args" \
	''"$APPROX"' -t' \
	2 \
	""

run_test "Missing option argument for -n at end of args" \
	''"$APPROX"' -n' \
	2 \
	""

run_test "Option order independence (-t0.80 -n1 vs -n1 -t0.80)" \
	'printf "receive\nrecieve\n" | '"$APPROX"' -t0.80 -n1 "receive"' \
	0 \
	"receive"

run_test "Option order reversed (-n1 -t0.80)" \
	'printf "receive\nrecieve\n" | '"$APPROX"' -n1 -t0.80 "receive"' \
	0 \
	"receive"

printf "========================================\n"
printf "POSIX Conformance: %d passed, %d failed\n" "$PASSED" "$FAILED"
printf "========================================\n"

if [ "$FAILED" -ne 0 ]; then
	exit 1
fi
exit 0
