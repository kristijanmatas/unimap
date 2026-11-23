#!/bin/sh
# Add .deb files to an apt repository directory and regenerate its signed
# indexes.  The directory is what gets published on GitHub Pages.
#
#   packaging/update-apt-repo.sh REPO_DIR file.deb [file.deb ...]
#
# Signing uses the default secret key in gpg (or $GPG_KEY_ID if set).
# Layout:  REPO_DIR/pool/main/u/unimap/*.deb
#          REPO_DIR/dists/stable/{Release,InRelease,Release.gpg}
#          REPO_DIR/dists/stable/main/binary-amd64/Packages{,.gz}
#          REPO_DIR/key.gpg   (public key users install)
set -eu

REPO=${1:?usage: update-apt-repo.sh REPO_DIR file.deb ...}
shift
SUITE=stable
COMP=main
ARCHS=amd64

mkdir -p "$REPO/pool/$COMP/u/unimap"
for deb in "$@"; do
    cp "$deb" "$REPO/pool/$COMP/u/unimap/"
done

HERE=$(cd "$(dirname "$0")" && pwd)
cd "$REPO"
for arch in $ARCHS; do
    d=dists/$SUITE/$COMP/binary-$arch
    mkdir -p "$d"
    apt-ftparchive --arch "$arch" packages pool > "$d/Packages"
    gzip -9nk -f "$d/Packages"
done

apt-ftparchive \
    -o APT::FTPArchive::Release::Origin=unimap \
    -o APT::FTPArchive::Release::Label=unimap \
    -o APT::FTPArchive::Release::Suite=$SUITE \
    -o APT::FTPArchive::Release::Codename=$SUITE \
    -o APT::FTPArchive::Release::Components=$COMP \
    -o "APT::FTPArchive::Release::Architectures=$ARCHS" \
    -o "APT::FTPArchive::Release::Description=unimap apt repository" \
    release dists/$SUITE > Release.tmp
mv Release.tmp dists/$SUITE/Release

set -- --batch --yes --pinentry-mode loopback
[ -n "${GPG_KEY_ID:-}" ] && set -- "$@" --local-user "$GPG_KEY_ID"
gpg "$@" --clearsign -o dists/$SUITE/InRelease dists/$SUITE/Release
gpg "$@" --armor --detach-sign -o dists/$SUITE/Release.gpg dists/$SUITE/Release
gpg --armor --export ${GPG_KEY_ID:-} > key.gpg

cp "$HERE/index.html" index.html 2>/dev/null || true
touch .nojekyll
