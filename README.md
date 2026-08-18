approx - non-interactive POSIX fuzzy stream filter and ranker
==============================================================
`approx` is a lightweight, zero-dependency POSIX CLI utility that scores,
filters, and ranks streaming text using normalized approximate string distance.

Unlike `grep` (which only supports exact substring or regex matches) or `fzf`
(which is an interactive TUI), `approx` is built for automated UNIX pipelines,
shell scripts, and resource-constrained embedded / IoT systems.

Features
--------
* **Zero external dependencies**: Strict C99 and standard POSIX libc headers.
* **Suckless philosophy**: Minimal code footprint, clean tabbed formatting, simple `arg.h`.
* **Memory bounded**: Space-optimized $O(M)$ DP matching (where $M$ is query length).
* **Streaming filter**: Real-time line-by-line streaming without buffering the entire input.
* **Top-N ranker**: Memory-bounded min-heap for outputting the top $N$ best matches sorted by score.
* **POSIX compliant**: Supports standard flags, stdin/file operands, and standard exit codes.

Requirements
------------
In order to build `approx` you need a C99 compiler and `make`.

Installation
------------
Edit `config.mk` to match your local setup (approx is installed into the
`/usr/local` namespace by default).

Afterwards enter the following command to build and install approx:

    make
    make install

Running tests
-------------
To execute the automated POSIX test suite:

    make test

Usage
-----
```
approx [-isveV] [-t threshold] [-n count] pattern [file ...]
```

### Options
* `-t threshold`: Minimum similarity score threshold between `0.00` and `1.00` (default: `0.70`).
* `-n count`: Output only top `count` matches sorted by score.
* `-s`: Prefix matching lines with their similarity score (`0.XX\t<line>`).
* `-i`: Match case-insensitively.
* `-v`: Invert match (select lines below threshold).
* `-e`: Perform exact full-line comparison instead of best substring match.
* `-V`: Print version information.

### Exit Status
* `0`: At least one matching line was selected.
* `1`: No lines were selected.
* `>1`: An error occurred.

Examples
--------
Filter log lines tolerating typos in "connection timeout":

    cat server.log | approx "connection timeout"

Find the top 5 closest matches with their scores from a wordlist:

    approx -n 5 -s "recieve" /usr/share/dict/words

Search case-insensitively with a strict 85% threshold:

    dmesg | approx -i -t 0.85 "out of memory"

Rank log errors in real-time pipelines:

    journalctl -u nginx -f | approx -t 0.80 "bad gateway"
