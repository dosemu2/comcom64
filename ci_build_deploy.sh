#!/bin/sh

set -e

sudo apt install -y dj64-dev-static
make both
ls -l src/32/comcom32.exe src/comcom64.exe
