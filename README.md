approx
======
approx filters and ranks text streams using fuzzy string matching. It reads
from standard input or files, scores lines against a pattern, and writes
matches to standard output.

It runs non-interactively in standard shell pipelines without external
dependencies.

[![Demo](assets/demo.gif)](https://asciinema.org/a/TeJ0ObmYHKKLoFo8)

Features
--------
* Strict C99 and standard POSIX libc headers only.
* Low memory use: dynamic programming table uses memory proportional to pattern length, not line length.
* Streams line by line without loading the input into memory.
* Bounded min-heap for top-N ranking.
* POSIX argument parsing and exit codes.
* Works on Linux, macOS, BSD, and Windows.

Installation
------------

### Build from source (recommended)

Requirements: a C99 compiler (gcc, clang, or tcc) and make.

```sh
git clone https://github.com/riccivr/approx.git
cd approx
make
sudo make install
```

To install into another directory such as `~/.local`:

```sh
make PREFIX="$HOME/.local" install
```

### Package managers

#### macOS and Linux (Homebrew)
```sh
brew install riccivr/tap/approx
```

#### Arch Linux (AUR)
```sh
yay -S approx
```

#### Debian and Ubuntu (.deb)
```sh
cd packaging/debian
dpkg-buildpackage -us -uc -b
sudo dpkg -i ../approx_*.deb
```

#### Windows (Scoop or Chocolatey)
```powershell
# Scoop
scoop bucket add approx https://github.com/riccivr/approx
scoop install approx

# Chocolatey
choco install approx
```

### Pre-compiled binaries
Static binaries for Linux, macOS, and Windows are attached to each release:

Download from [GitHub Releases](https://github.com/riccivr/approx/releases)

Running tests
-------------
The test suite covers unit tests, POSIX argument parsing, malformed streams,
metric invariants, sanitizers, and throughput:

    make test              # Core unit tests
    make test-posix        # POSIX argument parsing tests
    make test-stress       # Fuzzing and stream edge cases
    make test-properties   # Mathematical invariant tests
    make test-all          # Run all test suites
    make test-valgrind     # Memory leak check under Valgrind
    make test-tcc          # Build and test with tcc
    make test-clang        # Build and test with clang
    make sanitize          # Run with AddressSanitizer and UBSan
    make bench             # Run throughput benchmark

Usage
-----
```
approx [-isveV] [-t threshold] [-n count] pattern [file ...]
```

### Options
* `-t threshold`: Minimum similarity score from 0.00 to 1.00 (default: 0.70).
* `-n count`: Output only the top N matches sorted by score.
* `-s`: Prefix matching lines with their score (0.XX\t<line>).
* `-i`: Case-insensitive matching.
* `-v`: Invert match (select lines below threshold).
* `-e`: Compare full lines instead of finding the best matching substring.
* `-V`: Print version.

### Exit status
* `0`: At least one matching line was selected.
* `1`: No lines were selected.
* `>1`: An error occurred.

Examples
--------
Filter log lines for typos in "connection timeout":

    cat server.log | approx "connection timeout"

Find the top 5 closest matches from a wordlist:

    approx -n 5 -s "recieve" /usr/share/dict/words

Case-insensitive search with an 85% threshold:

    dmesg | approx -i -t 0.85 "out of memory"

Rank log errors in a stream:

    journalctl -u nginx -f | approx -t 0.80 "bad gateway"
