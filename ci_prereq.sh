#!/bin/sh

sudo apt update
sudo apt install -y \
  devscripts \
  equivs

add_ppa() {
  PPA="$1"
  KEY_ID="$2"
  USER=$(echo "$PPA" | cut -d: -f2 | cut -d/ -f1)
  NAME=$(echo "$PPA" | cut -d/ -f2)
  PPA_NAME="${USER}-ubuntu-${NAME}-jammy"

  if sudo add-apt-repository -y "$PPA"; then
    return 0
  fi

  echo "add-apt-repository failed for $PPA, using direct key import fallback..."
  if [ -n "$KEY_ID" ]; then
    sudo gpg --homedir /tmp --no-default-keyring --keyring "/etc/apt/trusted.gpg.d/${USER}.gpg" --keyserver hkps://keyserver.ubuntu.com --recv-keys "$KEY_ID" || true
  fi
  echo "deb https://ppa.launchpadcontent.net/$USER/$NAME/ubuntu jammy main" | sudo tee "/etc/apt/sources.list.d/${PPA_NAME}.list"
}

add_ppa ppa:stsp-0/thunk-gen 6B1BC3F1D8709B5A217DE0455ED4B4F6784FCB26
add_ppa ppa:stsp-0/dj64 6B1BC3F1D8709B5A217DE0455ED4B4F6784FCB26

sudo apt update
mk-build-deps --install --root-cmd sudo
