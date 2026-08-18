approx - non-interactive POSIX fuzzy stream filter and ranker
==============================================================
`approx` is a lightweight, zero-dependency POSIX CLI utility that scores,
filters, and ranks streaming text using normalized approximate string distance.

Unlike `grep` (which only supports exact substring or regex matches) or `fzf`
(which is an interactive TUI), `approx` is built for automated UNIX pipelines,
shell scripts, and resource-constrained embedded / IoT systems across Linux,
macOS, BSD, and Windows.

Features
--------
* **Zero external dependencies**: Strict C99 and standard POSIX libc headers.
* **Suckless philosophy**: Minimal code footprint, clean tabbed formatting, simple `arg.h`.
* **Cross-platform**: First-class support for Linux, macOS (Universal binary), BSD, and Windows.
* **Memory bounded**: Space-optimized $O(M)$ DP matching (where $M$ is query length).
* **Streaming filter**: Real-time line-by-line streaming without buffering the entire input.
* **Top-N ranker**: Memory-bounded min-heap for outputting the top $N$ best matches sorted by score.
* **POSIX compliant**: Supports standard flags, stdin/file operands, and standard exit codes.

Installation
------------

### 1. Build from Source (Recommended — "You do it")
In true suckless tradition, building from source gives you the cleanest,
fastest, zero-overhead binary with no package manager bloat.

Requirements: A C99 compiler (`gcc`, `clang`, or `tcc`) and `make`.

```sh
# Clone the repository
git clone https://github.com/riccivr/approx.git
cd approx

# Build and install (defaults to /usr/local)
make
sudo make install
```

To install into a custom location (e.g. `~/.local`):

```sh
make PREFIX="$HOME/.local" install
```

---

### 2. Package Managers

If you prefer using a system package manager:

#### macOS / Linux (Homebrew)
```sh
brew install riccivr/tap/approx
```
*(Or install directly from the included formula in `packaging/homebrew/approx.rb`)*

#### Arch Linux (AUR)
```sh
yay -S approx
# or
paru -S approx
```

#### Debian / Ubuntu (PPA / .deb)
```sh
# Build a native .deb package locally:
cd packaging/debian
dpkg-buildpackage -us -uc -b
sudo dpkg -i ../approx_*.deb
```

#### Windows (Scoop / Chocolatey)
```powershell
# Via Scoop
scoop bucket add approx https://github.com/riccivr/approx
scoop install approx

# Via Chocolatey
choco install approx
```

---

### 3. Pre-compiled Release Binaries
Pre-built standalone static binaries for the 3 major platforms are published
with every GitHub release:

* **Linux (x86_64 / arm64)**: `approx-linux-amd64.tar.gz`
* **macOS (Universal x86_64 + Apple Silicon ARM64)**: `approx-darwin-universal.tar.gz`
* **Windows (x86_64)**: `approx-windows-amd64.zip`

👉 Download from **[GitHub Releases](https://github.com/riccivr/approx/releases)**

Running tests
-------------
`approx` includes a multi-tiered test suite covering POSIX conformance,
stress/fuzzing, metric mathematical properties, memory safety, and benchmarks:

    make test              # Run core unit tests (24 tests)
    make test-posix        # Run IEEE 1003.1 syntax & argument parsing tests (13 tests)
    make test-stress       # Run fuzzing & malformed stream tests (9 tests)
    make test-properties   # Run metric invariant & symmetry tests (7 tests)
    make test-all          # Run all 4 test suites (53 tests passed)
    make test-valgrind     # Run leak check under Valgrind (0 leaks)
    make test-tcc          # Compile & test with Tiny C Compiler (tcc)
    make test-clang        # Compile & test with Clang
    make sanitize          # Compile & test under ASan + UBSan
    make bench             # Run throughput benchmark (100k lines: ~350k lines/sec)

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
