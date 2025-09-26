#!/bin/sh

set -e

make deb
sudo dpkg -i ../comcom64*.deb
