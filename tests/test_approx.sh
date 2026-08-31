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
tmp1="approx_test_1.$$.tmp"
tmp2="approx_test_2.$$.tmp"
trap 'rm -f "$tmp1" "$tmp2"' EXIT INT TERM

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

# Test 11: Error handling on invalid arguments (exit code 2)
run_test "Non-existent file argument returns exit code 2" \
	''"$APPROX"' "test" "/tmp/nonexistent_file_$$"' \
	2 \
	""

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
	"approx-1.1.0"

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

# Test 16: Pipeline early termination (SIGPIPE / broken pipe)
run_test "Early pipe closure handling (e.g. head -n 1)" \
	'printf "repeated test match line\nrepeated test match line\nrepeated test match line\n" | '"$APPROX"' "repeated" | head -n 1' \
	0 \
	"repeated test match line"

# Test 17: Quiet mode (-q)
run_test "Quiet mode exits 0 on match with no stdout" \
	'printf "match line\nother\n" | '"$APPROX"' -q "match"' \
	0 \
	""

run_test "Quiet mode exits 1 on no match with no stdout" \
	'printf "other line\n" | '"$APPROX"' -q "nonexistent"' \
	1 \
	""

# Test 18: Count mode (-c)
run_test "Count mode outputs matching line count" \
	'printf "match one\nskip\nmatch two\n" | '"$APPROX"' -c "match"' \
	0 \
	"2"

run_test "Count mode outputs 0 and exits 1 on no match" \
	'printf "skip one\nskip two\n" | '"$APPROX"' -c "match"' \
	1 \
	"0"

# Test 19: Max matches (-m)
run_test "Max count limits stream output" \
	'printf "one\ntwo\nthree\n" | '"$APPROX"' -m 2 -t 0.0 "query"' \
	0 \
	"$(printf 'one\ntwo')"

# Test 20: Files with/without matches (-l and -L)
tmp_pat="approx_test_pat.$$.tmp"
trap 'rm -f "$tmp1" "$tmp2" "$tmp_pat"' EXIT INT TERM

run_test "List files with matches (-l)" \
	''"$APPROX"' -l "alpha" '"$tmp1"' '"$tmp2"'' \
	0 \
	"$tmp1"

run_test "List files without matches (-L)" \
	''"$APPROX"' -L "alpha" '"$tmp1"' '"$tmp2"'' \
	0 \
	"$tmp2"

# Test 21: Filename header control (-H and -h)
run_test "Force filename prefix with -H" \
	'printf "match\n" | '"$APPROX"' -H "match"' \
	0 \
	"(standard input):match"

run_test "Suppress filename prefix with -h" \
	''"$APPROX"' -H -h "alpha" '"$tmp1"'' \
	0 \
	"alpha connection"

# Test 22: Field and delimiter filtering (-k and -d)
run_test "Delimited CSV field matching (-d, -k 2)" \
	'printf "101,john_doe,engineer\n102,jane_smith,designer\n" | '"$APPROX"' -d, -k 2 "jhn_doe"' \
	0 \
	"101,john_doe,engineer"

run_test "Whitespace field matching (-k 2)" \
	'printf "2026-08-31 INFO server_start\n2026-08-31 ERROR connection_drop\n" | '"$APPROX"' -k 2 "ERR"' \
	0 \
	"2026-08-31 ERROR connection_drop"

# Test 23: Damerau-Levenshtein transposition (-D)
run_test "Standard Levenshtein fails on transposition at high threshold" \
	'printf "recieve\n" | '"$APPROX"' -t 0.80 "receive"' \
	1 \
	""

run_test "Damerau-Levenshtein succeeds on transposition with -D" \
	'printf "recieve\n" | '"$APPROX"' -D -t 0.80 "receive"' \
	0 \
	"recieve"

# Test 24: Multi-pattern file search (-F)
printf "database\ntimeout\n" > "$tmp_pat"
run_test "Multi-pattern search from file (-F)" \
	'printf "server started\ndatabase error\nnetwork timeout\nother\n" | '"$APPROX"' -F '"$tmp_pat"'' \
	0 \
	"$(printf 'database error\nnetwork timeout')"

# Test 25: ANSI color match highlighting (-C)
run_test "ANSI match highlighting (-C)" \
	'printf "quick brown fox\n" | '"$APPROX"' -C "brown"' \
	0 \
	"$(printf 'quick \033[1;31mbrown\033[0m fox')"

printf "========================================\n"
printf "Tests passed: %d, Failed: %d\n" "$PASSED" "$FAILED"
printf "========================================\n"

if [ "$FAILED" -ne 0 ]; then
	exit 1
fi
exit 0
