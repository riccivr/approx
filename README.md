approx
======
approx is a non-interactive, typo-tolerant stream filter and ranker. It reads
from standard input or files, scores lines against a search pattern using
semi-global Levenshtein or Damerau-Levenshtein distance, and writes matching
lines to standard output.

It runs in standard shell pipelines without external dependencies, and can
also be embedded directly into C and C++ projects as an `stb`-style
single-header library (`approx.h`).

[![Demo](assets/demo.gif)](https://asciinema.org/a/tVerOgwr5zF6CJGM)

How it Works
------------
Unlike interactive subsequence fuzzy-finders (such as `fzf`), `approx` is designed
for shell pipelines and log analysis where typos in words or phrases need to be
tolerated.

By default, `approx` performs semi-global ("fuzzy contains") matching: prefixes
and suffixes in the line are free, and edit penalties are paid only to align the
query pattern against the closest matching substring:

$$\text{similarity} = 1 - \frac{\text{edit\_distance}}{|\text{pattern}|}$$

* Matching is byte-oriented; case-insensitivity (`-i`) performs ASCII case-folding.
* Memory usage scales with pattern length ($O(M)$), streaming arbitrarily long lines.

Features
--------
* Strict C99 and standard POSIX libc headers only.
* Single-header library (`approx.h`): drop into any C or C++ project without build systems.
* Low memory use: dynamic programming table uses memory proportional to pattern length, not line length.
* Streams line by line without loading the input into memory.
* Bounded min-heap for top-N ranking.
* Automatic filename prefixing on multiple files (grep-compatible `-H` / `-h` control).
* Target specific columns with custom field delimiters (`-k` and `-d`).
* Damerau-Levenshtein adjacent transposition support (`-D`).
* Multi-pattern search from files (`-F`).
* ANSI color match highlighting (`-C`).
* Works on Linux, macOS, BSD, and Windows.

C / C++ Library (`approx.h`)
----------------------------
`approx.h` is an `stb`-style single-header library. Drop `approx.h` into your project.

In **exactly one** `.c` or `.cpp` file:
```c
#define APPROX_IMPLEMENTATION
#include "approx.h"
```

In other files, simply `#include "approx.h"`.

### Example

```c
#define APPROX_IMPLEMENTATION
#include "approx.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *query = "receive";
    const char *target = "recieve";
    size_t start = 0, end = 0;

    /* Damerau-Levenshtein similarity with span tracking */
    double score = approx_sim_span(query, strlen(query),
                                   target, strlen(target),
                                   APPROX_DAMERAU,
                                   &start, &end);

    printf("Similarity: %.2f (span: %zu..%zu)\n", score, start, end);
    return 0;
}
```

Installation
------------

### Build from source (recommended)

Requirements: a C99 compiler (`gcc`, `clang`, or `tcc`) and `make`.

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

### Pre-compiled binaries
Static binaries for Linux and Windows are attached to each release:

Download from [GitHub Releases](https://github.com/riccivr/approx/releases)

### Packaging recipes
Package definitions for distribution maintainers and local package builds are provided under `packaging/`:
* `packaging/debian/`: Debian / Ubuntu packaging (`dpkg-buildpackage`)
* `packaging/aur/`: Arch Linux `PKGBUILD`
* `packaging/homebrew/`: Homebrew formula (`approx.rb`)
* `packaging/scoop/`: Scoop manifest (`approx.json`)
* `packaging/chocolatey/`: Chocolatey package (`approx.nuspec`)

Running tests
-------------
The test suite covers unit tests, POSIX argument parsing, malformed streams,
metric invariants, C/C++ embedding, sanitizers, and throughput:

    make test              # Core unit tests
    make test-posix        # POSIX argument parsing tests
    make test-stress       # Fuzzing and stream edge cases
    make test-properties   # Mathematical invariant tests
    make test-examples     # C library embedding test
    make test-cpp          # C++ library embedding test
    make test-all          # Run all test suites
    make test-valgrind     # Memory leak check under Valgrind
    make test-tcc          # Build and test with tcc
    make test-clang        # Build and test with clang
    make sanitize          # Run with AddressSanitizer and UBSan
    make bench             # Run throughput benchmark

Usage
-----
```
approx [-cCDehHilLmqsvV] [-t threshold] [-n count] [-m max] [-d delim] [-k field] [-F file] [pattern] [file ...]
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

```sh
$ cat server.log | approx "connection timeout"
2026-08-31 20:11:02 [ERROR] connectoin timeout to db-primary
2026-08-31 20:14:55 [ERROR] connection timed out after 30s
```

Target column 2 of a CSV file for typoed user names:

```sh
$ approx -d, -k 2 "john_doe" users.csv
101,jhn_doe,engineer,active
109,john_doe_99,manager,active
```

Tolerate swapped letters with Damerau-Levenshtein:

```sh
$ approx -D -t 0.85 "receive" /usr/share/dict/words
receive
recieve
```

Find the top 5 closest matches with similarity scores:

```sh
$ approx -n 5 -s "recieve" /usr/share/dict/words
1.00	recieve
0.86	receive
0.86	relieve
0.71	recede
0.71	recipe
```

Search against multiple patterns from a file:

```sh
$ cat patterns.txt
database error
connection timeout

$ approx -F patterns.txt server.log
2026-08-31 19:40:11 [WARN] databse error: pool exhausted
2026-08-31 20:11:02 [ERROR] connectoin timeout to db-primary
```

Quiet check in shell conditionals:

```sh
if approx -q "FATAL" /var/log/syslog; then
    notify-send "Server error detected"
fi
```
