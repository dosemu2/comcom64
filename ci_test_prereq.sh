#!/bin/sh

sudo add-apt-repository ppa:jwt27/djgpp-toolchain
sudo add-apt-repository ppa:dosemu2/ppa

sudo apt update -q

sudo apt install -y gcc-djgpp dosemu2 dj64-dev-static
