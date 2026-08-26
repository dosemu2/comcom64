#!/bin/sh

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

add_ppa ppa:dosemu2/ppa

sudo apt update -q

sudo apt install -y dosemu2 dj64-dev-static
