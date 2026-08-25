#!/bin/sh

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

add_ppa ppa:dosemu2/ppa EBE1B5DED2AD45D6

sudo apt update -q

sudo apt install -y dosemu2 dj64-dev-static
