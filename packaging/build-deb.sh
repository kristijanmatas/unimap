#!/bin/sh
# Build a .deb around a statically linked unimap binary.
# Being static, the package installs on any Debian / Ubuntu / Kali release.
#
#   packaging/build-deb.sh VERSION [OUTDIR]
#
# Writes OUTDIR/unimap_VERSION_amd64.deb (OUTDIR defaults to ./dist).
set -eu

VERSION=${1:?usage: build-deb.sh VERSION [OUTDIR]}
VERSION=${VERSION#v}
OUTDIR=${2:-dist}
ARCH=amd64

SRC=$(cd "$(dirname "$0")/.." && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
ROOT=$WORK/root

cc -O2 -Wall -Wextra -static -s -o "$WORK/unimap" "$SRC/unimap.c"

install -d "$ROOT/DEBIAN" "$ROOT/usr/bin" "$ROOT/usr/share/man/man1" \
           "$ROOT/usr/share/doc/unimap"
install -m 755 "$WORK/unimap" "$ROOT/usr/bin/unimap"
gzip -9n < "$SRC/unimap.1" > "$ROOT/usr/share/man/man1/unimap.1.gz"
install -m 644 "$SRC/debian/copyright" "$ROOT/usr/share/doc/unimap/copyright"
gzip -9n < "$SRC/debian/changelog" > "$ROOT/usr/share/doc/unimap/changelog.gz"
chmod 644 "$ROOT/usr/share/man/man1/unimap.1.gz" "$ROOT/usr/share/doc/unimap/changelog.gz"

SIZE=$(du -sk "$ROOT/usr" | cut -f1)
cat > "$ROOT/DEBIAN/control" <<EOF
Package: unimap
Version: $VERSION
Architecture: $ARCH
Maintainer: Kristijan Matas <kristijan.matas@gmail.com>
Installed-Size: $SIZE
Suggests: wl-clipboard | xclip | xsel
Section: utils
Priority: optional
Homepage: https://github.com/kristijanmatas/unimap
Description: Unicode character map for the terminal
 unimap is an interactive Unicode character map for UTF-8 terminals, in the
 spirit of GNOME Character Map. It lists every Unicode block next to a
 character grid, searches characters by name, collects characters into a
 text buffer and copies them to the clipboard.
 .
 It is written in plain C without ncurses, supports the mouse and vim-style
 keys, and comes with several color schemes.
EOF

mkdir -p "$OUTDIR"
dpkg-deb --root-owner-group -Zxz --build "$ROOT" "$OUTDIR/unimap_${VERSION}_${ARCH}.deb"
