#!/bin/sh

sudo add-apt-repository ppa:dosemu2/ppa

sudo apt update -q

sudo apt install -y dosemu2

sudo dpkg -i ../comcom64*.deb
