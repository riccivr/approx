#!/bin/sh
set -e

VERSION="1.1.0"
DIST_DIR="/tmp/release-${VERSION}"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/linux" "$DIST_DIR/windows" "$DIST_DIR/dist"

# Linux amd64
gcc -std=c99 -pedantic -Wall -Wextra -Os \
    -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L \
    -DVERSION=\"${VERSION}\" approx.c -o "$DIST_DIR/linux/approx" -lm
cp LICENSE README.md approx.1 "$DIST_DIR/linux/"
tar -czf "$DIST_DIR/dist/approx-linux-amd64.tar.gz" -C "$DIST_DIR/linux" .

# Windows amd64
x86_64-w64-mingw32-gcc -std=c99 -Wall -Wextra -Os \
    -DVERSION=\"${VERSION}\" approx.c -o "$DIST_DIR/windows/approx.exe"
cp LICENSE README.md "$DIST_DIR/windows/"
(cd "$DIST_DIR/windows" && zip -q "$DIST_DIR/dist/approx-windows-amd64.zip" approx.exe LICENSE README.md)

# Source dist
make dist
cp "approx-${VERSION}.tar.gz" "$DIST_DIR/dist/"

# Checksums
(cd "$DIST_DIR/dist" && sha256sum * > SHA256SUMS.txt)

echo "Release assets generated at $DIST_DIR/dist:"
ls -lh "$DIST_DIR/dist"
cat "$DIST_DIR/dist/SHA256SUMS.txt"
