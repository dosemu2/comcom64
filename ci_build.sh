#!/bin/sh

set -e

make deb

make 32 -j 9
