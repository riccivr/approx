#!/usr/bin/env bash
#
# Terminal demonstration script for approx asciinema recording
#

set -e

# Setup sample data
cat << 'EOF' > /tmp/sample_server.log
2026-08-19 14:00:01 [INFO]  worker pool initialized with 16 threads
2026-08-19 14:00:05 [INFO]  database connection pool established
2026-08-19 14:00:12 [WARN]  slow database query detected: SELECT * FROM orders (342ms)
2026-08-19 14:00:19 [ERROR] conection timeout to redis cluster on 10.0.4.12
2026-08-19 14:00:23 [INFO]  retry connection succeeded for redis worker
2026-08-19 14:00:31 [ERROR] connection timed out after 5000ms: payment gateway
2026-08-19 14:00:45 [WARN]  recieve buffer overflow on socket fd=42
2026-08-19 14:00:52 [FATAL] FATAL ERROR: out of memory allocating 4GB buffer
2026-08-19 14:01:00 [INFO]  health check passed (200 OK)
EOF

cat << 'EOF' > /tmp/sample_words.txt
receive
receipt
recipe
deceive
perceive
receptive
EOF

# Formatting helpers
BLUE='\033[1;34m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
CYAN='\033[1;36m'
RESET='\033[0m'
BOLD='\033[1m'

type_cmd() {
    local cmd="$1"
    printf "${GREEN}ricci@tower${RESET}:${BLUE}~/approx${RESET}$ "
    sleep 0.4
    for ((i=0; i<${#cmd}; i++)); do
        printf "${BOLD}%s${RESET}" "${cmd:$i:1}"
        sleep 0.03
    done
    printf "\n"
    sleep 0.3
}

clear
sleep 0.6

printf "${CYAN}=== approx: Non-interactive Fuzzy Stream Filter Demonstration ===${RESET}\n\n"
sleep 1.0

# Demo 1: Typo tolerance in log searching
type_cmd "cat server.log | ./approx \"connection timeout\""
cat /tmp/sample_server.log | ./approx "connection timeout"
sleep 1.6
printf "\n"

# Demo 2: Score prefix with -s
type_cmd "./approx -s \"receive\" words.txt"
./approx -s "receive" /tmp/sample_words.txt
sleep 1.6
printf "\n"

# Demo 3: Top-N ranking with -n
type_cmd "./approx -n 3 -s \"database query\" server.log"
./approx -n 3 -s "database query" /tmp/sample_server.log
sleep 1.8
printf "\n"

# Demo 4: Case-insensitive search with -i
type_cmd "./approx -i \"fatal error\" server.log"
./approx -i "fatal error" /tmp/sample_server.log
sleep 1.6
printf "\n"

# Demo 5: Inverted matching with -v
type_cmd "./approx -v -t 0.50 \"INFO\" server.log | head -n 3"
./approx -v -t 0.50 "INFO" /tmp/sample_server.log | head -n 3
sleep 1.8
printf "\n"

# Demo 6: High throughput streaming benchmark
type_cmd "make bench"
make bench
sleep 2.0
printf "\n"

printf "${GREEN}✓ Done! Clean, zero-dependency POSIX fuzzy filtering.${RESET}\n"
sleep 1.5

# Cleanup
rm -f /tmp/sample_server.log /tmp/sample_words.txt
