#!/bin/sh
#
# Metric Invariant and Property-Based Tests for approx
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
printf "Running Metric Invariant & Property Tests\n"
printf "========================================\n"

# Invariant 1: Identity Property (sim(X, X) == 1.00)
run_test "Identity property on ASCII string" \
	'printf "mathematics\n" | '"$APPROX"' -s "mathematics"' \
	0 \
	"$(printf '1.00\tmathematics')"

run_test "Identity property on exact full-line mode (-e)" \
	'printf "exact line content\n" | '"$APPROX"' -e -s "exact line content"' \
	0 \
	"$(printf '1.00\texact line content')"

# Invariant 2: Monotonicity Property (exact > 1 edit > 2 edits > 3 edits)
run_test "Score monotonicity: 0 vs 1 vs 2 edits on 5-char pattern" \
	'printf "apple\nappla\nappza\n" | '"$APPROX"' -t 0.0 -s "apple"' \
	0 \
	"$(printf '1.00\tapple\n0.80\tappla\n0.60\tappza')"

# Invariant 3: Bounded Metric Range (0.00 <= score <= 1.00)
run_test "Disjoint string has score 0.00 (bounded below)" \
	'printf "zzzzz\n" | '"$APPROX"' -t 0.0 -s "aaaaa"' \
	0 \
	"$(printf '0.00\tzzzzz')"

# Invariant 4: Substring Invariant
# The presence of surrounding padding in a line must NOT decrease score in substring mode
run_test "Substring invariant: padding does not decrease score in substring mode" \
	'printf "target\n[prefix] target [suffix]\n" | '"$APPROX"' -s "target"' \
	0 \
	"$(printf '1.00\ttarget\n1.00\t[prefix] target [suffix]')"

# Invariant 5: Top-N Stable Tie-breaking
# When scores are identical, stream arrival order (line number) is preserved
run_test "Top-N stable tie-breaking preserves line order" \
	'printf "first identical\nsecond identical\nthird identical\n" | '"$APPROX"' -n 3 "identical"' \
	0 \
	"$(printf 'first identical\nsecond identical\nthird identical')"

# Invariant 6: Symmetry in Exact Match Mode
run_test "Symmetry of similarity score in exact mode (-e)" \
	's1=$(printf "cat\n" | '"$APPROX"' -e -t 0.0 -s "bat" | cut -f1); s2=$(printf "bat\n" | '"$APPROX"' -e -t 0.0 -s "cat" | cut -f1); [ "$s1" = "$s2" ] && echo match' \
	0 \
	"match"

printf "========================================\n"
printf "Metric Invariants: %d passed, %d failed\n" "$PASSED" "$FAILED"
printf "========================================\n"

if [ "$FAILED" -ne 0 ]; then
	exit 1
fi
exit 0
