#!/bin/sh

set -e

# Otherwise the binaries get an old timestamp
dch -i -m "Github Actions build"

make deb

git checkout debian/changelog
