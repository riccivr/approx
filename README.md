approx
======
approx filters and ranks text streams using fuzzy string matching. It reads
from standard input or files, scores lines against a pattern, and writes
matches to standard output.

It runs non-interactively in standard shell pipelines without external
dependencies.

[![Demo](assets/demo.gif)](https://asciinema.org/a/5epMdgc4RpC4G3OB)

Features
--------
* Strict C99 and standard POSIX libc headers only.
* Low memory use: dynamic programming table uses memory proportional to pattern length, not line length.
* Streams line by line without loading the input into memory.
* Bounded min-heap for top-N ranking.
* POSIX argument parsing and exit codes.
* Target specific columns with custom field delimiters.
* Damerau-Levenshtein transposition support.
* Multi-pattern search from files.
* ANSI color match highlighting.
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
approx [-cCDhHilLmqsvV] [-t threshold] [-n count] [-m max] [-d delim] [-k field] [-F file] [pattern] [file ...]
```

### Options
* `-t threshold`: Minimum similarity score from 0.00 to 1.00 (default: 0.70).
* `-n count`: Output only the top N matches sorted by score.
* `-m max`: Stop reading after finding `max` matches.
* `-q`: Quiet mode; exit 0 on first match, exit 1 on no match, suppress standard output.
* `-c`: Output only the count of matching lines.
* `-l`: Print only names of files with matching lines.
* `-L`: Print only names of files without matching lines.
* `-H`: Print filename prefix for each match.
* `-h`: Suppress filename prefix in output.
* `-k field`: Compare similarity against 1-based field number while outputting the full line.
* `-d delim`: Delimiter character for `-k` (default: whitespace).
* `-D`: Enable Damerau-Levenshtein distance (adjacent transposition counts as 1 edit).
* `-F file`: Read search patterns from `file`, one per line.
* `-C`: Highlight matched substrings with ANSI colors.
* `-s`: Prefix matching lines with their score (`0.XX\t<line>`).
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

Target column 2 of a CSV file for typoed user names:

    approx -d, -k 2 "john_doe" users.csv

Tolerate swapped letters with Damerau-Levenshtein:

    approx -D -t 0.85 "receive" /usr/share/dict/words

Quiet check in shell conditionals:

    if approx -q "FATAL" /var/log/syslog; then
        notify-send "Server error detected"
    fi

Find the top 5 closest matches from a wordlist:

    approx -n 5 -s "recieve" /usr/share/dict/words

Search against multiple patterns from a file:

    approx -F patterns.txt server.log
