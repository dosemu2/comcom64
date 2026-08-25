#!/bin/sh

sudo apt update
sudo apt install -y \
  devscripts \
  equivs

add_ppa() {
  PPA="$1"
  for i in 1 2 3 4 5 6 7 8 9 10; do
    echo "Adding $PPA (attempt $i)..."
    if sudo add-apt-repository -y "$PPA"; then
      return 0
    fi
    sleep 8
  done
  return 1
}

add_ppa ppa:stsp-0/thunk-gen
add_ppa ppa:stsp-0/dj64

sudo apt update
mk-build-deps --install --root-cmd sudo
