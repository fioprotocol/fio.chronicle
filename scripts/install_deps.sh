#!/usr/bin/env bash

if [[ "$EUID" -ne 0 ]]; then
  echo "ERROR: Script must be run as root! Use sudo command as follows; sudo ./<script name>"
  echo
  exit 1
fi

apt-get update
apt-get update --fix-missing
DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get -y install tzdata
apt-get -y install zip unzip libncurses5 wget git build-essential cmake curl libcurl4-gnutls-dev libgmp-dev libssl-dev libusb-1.0.0-dev libzstd-dev time pkg-config zlib1g-dev libtinfo-dev bzip2 libbz2-dev python3 file
