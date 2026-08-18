#!/bin/sh
#
# Throughput Benchmark for approx
#

APPROX="./approx"
LINES=100000

printf "========================================\n"
printf "Generating %d synthetic log lines...\n" "$LINES"
printf "========================================\n"

# Generate 100,000 lines to a temp file
bench_file="/tmp/approx_bench_$$.log"
trap 'rm -f "$bench_file"' EXIT INT TERM

awk -v n="$LINES" 'BEGIN {
	for (i = 1; i <= n; i++) {
		if (i % 1000 == 0)
			printf "2026-08-18 23:00:00 [ERROR] connection timeout on worker %d\n", i;
		else
			printf "2026-08-18 23:00:00 [INFO] request id=%d status=200 path=/api/v1/resource latency=12ms\n", i;
	}
}' > "$bench_file"

file_bytes=$(wc -c < "$bench_file")
file_mb=$(awk -v b="$file_bytes" 'BEGIN { printf "%.2f", b / (1024 * 1024) }')

printf "Benchmark dataset: %s MB (%d lines)\n\n" "$file_mb" "$LINES"

printf "Running benchmark: approx \"connection timeout\" (substring match)...\n"
start_time=$(date +%s%N 2>/dev/null || date +%s)
"$APPROX" "connection timeout" "$bench_file" > /dev/null
end_time=$(date +%s%N 2>/dev/null || date +%s)

# Compute elapsed time in seconds
if [ ${#start_time} -gt 10 ]; then
	# Nanoseconds available
	elapsed_sec=$(awk -v s="$start_time" -v e="$end_time" 'BEGIN { printf "%.3f", (e - s) / 1000000000 }')
else
	# Seconds fallback
	elapsed_sec=$((end_time - start_time))
	[ "$elapsed_sec" -eq 0 ] && elapsed_sec=1
fi

lines_per_sec=$(awk -v n="$LINES" -v t="$elapsed_sec" 'BEGIN { if (t > 0) printf "%d", n / t; else print "N/A" }')
mb_per_sec=$(awk -v m="$file_mb" -v t="$elapsed_sec" 'BEGIN { if (t > 0) printf "%.2f", m / t; else print "N/A" }')

printf "Elapsed time  : %s s\n" "$elapsed_sec"
printf "Throughput    : %s lines/sec\n" "$lines_per_sec"
printf "Data Rate     : %s MB/s\n" "$mb_per_sec"
printf "========================================\n"
