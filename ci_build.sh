#!/bin/sh

set -e

dch -i -m "Github Actions build"
make deb
git checkout debian/changelog
