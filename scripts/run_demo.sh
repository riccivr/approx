#!/usr/bin/env bash
#
# Terminal demonstration script for approx v1.1.0 asciinema recording
#

set -e
export PATH="$HOME/.local/bin:$PATH"

# Setup realistic sample files in temporary demo directory
mkdir -p /tmp/approx_demo
cd /tmp/approx_demo

cat << 'EOF' > syslog
2026-08-31 14:00:01 kernel: [   12.304102] EXT4-fs (sda2): mounted filesystem with ordered data mode
2026-08-31 14:00:03 systemd[1]: Started nginx.service - A high performance web server.
2026-08-31 14:00:15 app-worker[4812]: [INFO] worker pool initialized with 16 threads
2026-08-31 14:00:19 app-worker[4812]: [ERROR] conection timeout to redis cluster on 10.0.4.12
2026-08-31 14:00:22 app-worker[4815]: [WARN] database connection retry attempt 1 of 3
2026-08-31 14:00:27 app-worker[4812]: [ERROR] connection timed out after 5000ms: payment gateway
2026-08-31 14:00:33 kernel: [   45.892011] eth0: link up, 1000Mbps, full-duplex
2026-08-31 14:00:41 app-worker[4819]: [ERROR] tcp socket conn timeout waiting for upstream ack
2026-08-31 14:00:55 app-worker[4812]: [ERROR] failed connection timeout on replica node 10.0.4.15
2026-08-31 14:01:04 systemd[1]: reload configuration complete
2026-08-31 14:01:18 app-worker[4820]: [ERROR] client request aborted: connection timeout
2026-08-31 14:01:30 app-worker[4812]: [INFO] health check routine passed (status 200)
EOF

cat << 'EOF' > users.csv
id,username,role,status
101,jhn_doe,engineer,active
102,jane_smith,designer,active
103,alice_wonder,product,active
104,bob_builder,devops,active
105,john_doe_99,manager,active
106,charlie_brown,qa,active
EOF

cat << 'EOF' > words.txt
receive
received
receiver
receives
receipt
receipts
recipe
recipes
deceive
deceived
perceive
perceived
receptive
reception
recital
EOF

cat << 'EOF' > app.log
2026-08-31 14:10:01 [WARN]  slow database query detected: SELECT * FROM orders WHERE status='pending' (480ms)
2026-08-31 14:10:04 [INFO]  cache hit ratio 94.2% over last 60 seconds
2026-08-31 14:10:09 [ERROR] database connection pool exhausted (max_pool_size=50)
2026-08-31 14:10:14 [ERROR] failed database query transaction rollback on order_id=98412
2026-08-31 14:10:22 [FATAL] unhandled database connection error: connection refused by postgres
2026-08-31 14:10:28 [WARN]  fallback cache store enabled for database queries
2026-08-31 14:10:35 [INFO]  background sync complete (1420 records processed)
2026-08-31 14:10:41 [ERROR] database write lock conflict detected in thread worker-03
2026-08-31 14:10:50 [INFO]  garbage collector completed in 1.4ms
EOF

cat << 'EOF' > patterns.txt
database error
connection timeout
EOF

# Formatting helpers
BLUE='\033[1;34m'
GREEN='\033[1;32m'
RESET='\033[0m'
BOLD='\033[1m'

type_cmd() {
    local cmd="$1"
    printf "${GREEN}ricci@desktop${RESET}:${BLUE}~${RESET}$ "
    sleep 0.30
    for ((i=0; i<${#cmd}; i++)); do
        printf "${BOLD}%s${RESET}" "${cmd:$i:1}"
        sleep 0.025
    done
    printf "\n"
    sleep 0.20
}

clear
sleep 0.4

# Demo 1: Version check
type_cmd "approx -V"
approx -V
sleep 1.2
printf "\n"

# Demo 2: Filter log stream with typo tolerance and ANSI match highlighting
type_cmd "cat syslog | approx -C \"connection timeout\""
cat syslog | approx -C "connection timeout"
sleep 1.6
printf "\n"

# Demo 3: Column targeting in CSV
type_cmd "approx -d, -k 2 \"john_doe\" users.csv"
approx -d, -k 2 "john_doe" users.csv
sleep 1.6
printf "\n"

# Demo 4: Damerau-Levenshtein transposition tolerance
type_cmd "approx -D -t 0.85 \"recieve\" words.txt"
approx -D -t 0.85 "recieve" words.txt
sleep 1.6
printf "\n"

# Demo 5: Top-N error ranking in application log
type_cmd "approx -n 5 -s \"database error\" app.log"
approx -n 5 -s "database error" app.log
sleep 1.6
printf "\n"

# Demo 6: Multi-pattern search from file
type_cmd "approx -F patterns.txt syslog"
approx -F patterns.txt syslog
sleep 1.6
printf "\n"

# Demo 7: Streaming benchmark
type_cmd "cd ~/src/approx && make bench"
cd /mnt/c/Users/ricci/Desktop/code/approx && make bench
sleep 2.0

# Cleanup demo data
rm -rf /tmp/approx_demo
