#!/usr/bin/env bash
#
# Terminal demonstration script for approx asciinema recording
#

set -e

# Setup realistic sample files
mkdir -p /tmp/approx_demo
cd /tmp/approx_demo

cat << 'EOF' > syslog
2026-08-19 14:00:01 kernel: [   12.304102] EXT4-fs (sda2): mounted filesystem with ordered data mode
2026-08-19 14:00:03 systemd[1]: Started nginx.service - A high performance web server.
2026-08-19 14:00:15 app-worker[4812]: [INFO] worker pool initialized with 16 threads
2026-08-19 14:00:19 app-worker[4812]: [ERROR] conection timeout to redis cluster on 10.0.4.12
2026-08-19 14:00:22 app-worker[4815]: [WARN] database connection retry attempt 1 of 3
2026-08-19 14:00:27 app-worker[4812]: [ERROR] connection timed out after 5000ms: payment gateway
2026-08-19 14:00:33 kernel: [   45.892011] eth0: link up, 1000Mbps, full-duplex
2026-08-19 14:00:41 app-worker[4819]: [ERROR] tcp socket conn timeout waiting for upstream ack
2026-08-19 14:00:55 app-worker[4812]: [ERROR] failed connection timeout on replica node 10.0.4.15
2026-08-19 14:01:04 systemd[1]: reload configuration complete
2026-08-19 14:01:18 app-worker[4820]: [ERROR] client request aborted: connection timeout
2026-08-19 14:01:30 app-worker[4812]: [INFO] health check routine passed (status 200)
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
2026-08-19 14:10:01 [WARN]  slow database query detected: SELECT * FROM orders WHERE status='pending' (480ms)
2026-08-19 14:10:04 [INFO]  cache hit ratio 94.2% over last 60 seconds
2026-08-19 14:10:09 [ERROR] database connection pool exhausted (max_pool_size=50)
2026-08-19 14:10:14 [ERROR] failed database query transaction rollback on order_id=98412
2026-08-19 14:10:22 [FATAL] unhandled database connection error: connection refused by postgres
2026-08-19 14:10:28 [WARN]  fallback cache store enabled for database queries
2026-08-19 14:10:35 [INFO]  background sync complete (1420 records processed)
2026-08-19 14:10:41 [ERROR] database write lock conflict detected in thread worker-03
2026-08-19 14:10:50 [INFO]  garbage collector completed in 1.4ms
EOF

cat << 'EOF' > auth.log
Aug 19 14:20:01 tower sshd[9102]: Invalid user admin from 192.168.1.105 port 54120
Aug 19 14:20:04 tower sshd[9105]: Failed password for invalid user root from 10.0.1.55 port 42100
Aug 19 14:20:10 tower sshd[9108]: Accepted publickey for ricci from 10.0.0.2 port 51230 ssh2
Aug 19 14:20:15 tower sshd[9112]: INVALID USER test from 172.16.0.42 port 38910
Aug 19 14:20:22 tower sshd[9118]: Connection closed by authenticating user Invalid User guest
Aug 19 14:20:30 tower sshd[9124]: Session opened for user ricci by (uid=0)
Aug 19 14:20:38 tower sshd[9130]: Failed password for invalid user ubuntu from 192.168.1.200
EOF

# Formatting helpers
BLUE='\033[1;34m'
GREEN='\033[1;32m'
RESET='\033[0m'
BOLD='\033[1m'

type_cmd() {
    local cmd="$1"
    printf "${GREEN}ricci@desktop${RESET}:${BLUE}~${RESET}$ "
    sleep 0.35
    for ((i=0; i<${#cmd}; i++)); do
        printf "${BOLD}%s${RESET}" "${cmd:$i:1}"
        sleep 0.03
    done
    printf "\n"
    sleep 0.25
}

clear
sleep 0.5

# Demo 1: Filter log stream with typo tolerance
type_cmd "cat syslog | approx \"connection timeout\""
cat syslog | approx "connection timeout"
sleep 1.6
printf "\n"

# Demo 2: Wordlist scoring and ranking
type_cmd "approx -n 6 -s \"recieve\" words.txt"
approx -n 6 -s "recieve" words.txt
sleep 1.6
printf "\n"

# Demo 3: Top-N error ranking in application log
type_cmd "approx -n 5 -s \"database error\" app.log"
approx -n 5 -s "database error" app.log
sleep 1.6
printf "\n"

# Demo 4: Case-insensitive search on auth stream
type_cmd "cat auth.log | approx -i \"invalid user\""
cat auth.log | approx -i "invalid user"
sleep 1.6
printf "\n"

# Demo 5: Inverted matching to filter out routine info logs
type_cmd "approx -v -t 0.45 \"INFO\" app.log"
approx -v -t 0.45 "INFO" app.log
sleep 1.6
printf "\n"

# Demo 6: Streaming benchmark
type_cmd "cd ~/src/approx && make bench"
cd /mnt/c/Users/ricci/Desktop/code/approx && make bench
sleep 2.0

# Cleanup demo data
rm -rf /tmp/approx_demo
