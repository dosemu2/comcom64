#!/bin/sh

sudo apt update
sudo apt install -y \
  devscripts \
  equivs

add_ppa() {
  PPA="$1"
  PPA_NAME=$(echo "$PPA" | sed 's#ppa:##' | tr '/' '-')
  for i in 1 2 3; do
    echo "Adding $PPA (attempt $i)..."
    if sudo add-apt-repository -y "$PPA"; then
      return 0
    fi
    sudo rm -f "/etc/apt/sources.list.d/${PPA_NAME}"*.list
    sleep 5
  done
  echo "Fallback: manually adding PPA $PPA..."
  USER=$(echo "$PPA" | cut -d: -f2 | cut -d/ -f1)
  NAME=$(echo "$PPA" | cut -d/ -f2)
  gpg --keyserver hkps://keyserver.ubuntu.com --recv-keys 6B1BC3F1D8709B5A217DE0455ED4B4F6784FCB26 2>/dev/null || true
  gpg --export 6B1BC3F1D8709B5A217DE0455ED4B4F6784FCB26 | sudo tee /etc/apt/trusted.gpg.d/stsp-0.gpg > /dev/null
  echo "deb https://ppa.launchpadcontent.net/$USER/$NAME/ubuntu jammy main" | sudo tee "/etc/apt/sources.list.d/stsp-0-ubuntu-$NAME-jammy.list"
}

add_ppa ppa:stsp-0/thunk-gen
add_ppa ppa:stsp-0/dj64

sudo apt update
mk-build-deps --install --root-cmd sudo
