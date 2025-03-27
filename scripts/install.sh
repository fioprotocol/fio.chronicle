#!/usr/bin/env bash

function usage() {
   printf "Usage: $0 OPTION...
  -i       Intall Directory (FIO.Chronicle binary). Default: /opt/fio-chronicle
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
SYSTEM_INSTALL=${SYSTEM_INSTALL:-false}
if [ $# -ne 0 ]; then
   while getopts "hdi:s" opt; do
      case "${opt}" in
      d)
         DEBUG=true
         set -x
         ;;
      i)
         INSTALL_DIR=$OPTARG
         ;;
      s)
         SYSTEM_INSTALL=true
         ;;
      h)
         usage
         ;;
      ?)
         echo "Invalid Option!" 1>&2
         usage
         ;;
      :)
         echo "Invalid Option: -${OPTARG} requires an argument." 1>&2
         usage
         ;;
      *)
         usage
         ;;
      esac
   done
fi

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

if [[ "$EUID" -eq 0 ]]; then
  echo && echo "ERROR: Script should not be run as root! Exiting..."
  echo
  exit 1
fi

groups $(id -un) | grep sudo >/dev/null
if [[ $? -ne 0 ]]; then
  echo
  echo "ERROR: User $(id -un) does NOT have sudo privilege! sudo privilege is required to run this script. Exiting..."
  echo
  exit 1
fi

if [[ "$(uname)" == "Linux" ]]; then
  if [[ -e /etc/os-release ]]; then
    # obtain NAME and other information
    . /etc/os-release
    if [[ ${NAME} != "Ubuntu" ]]; then
      echo && echo "Currently only supporting Ubuntu based builds. Proceed at your own risk."
      pause
    fi
  else
    echo && echo "Currently only supporting Ubuntu based builds. /etc/os-release not found. Your Linux distribution is not supported. Proceed at your own risk."
    pause
  fi
else
  echo && echo "Currently only supporting Ubuntu based builds. Your architecture is not supported. Proceed at your own risk."
  pause
fi

echo && echo "Installing Fio.Chronicle..."

PROJECT_DIR=$(pwd)
BUILD_DIR=${PROJECT_DIR}/build
CONFIG_DIR=${PROJECT_DIR}/config

if [[ ! -e ${BUILD_DIR}/chronicle-receiver ]]; then
  echo && echo "ERROR: Unable to install FIO.Chronicle; ${BUILD_DIR}/chronicle-receiver does not exist!" && echo
  exit 1
fi

# Do either;
# local install (into /opt)
# system install (/usr/local/bin, etc/fio/chronicle, /var/lib/fio-chronicle)
if ! $SYSTEM_INSTALL; then
  makedir /opt/fio-chronicle
  makedir /opt/fio-chronicle/config
  makedir /opt/fio-chronicle/data
  makedir /opt/fio-chronicle/log

  cp ${BUILD_DIR}/chronicle-receiver /opt/fio-chronicle

  if [[ -e /opt/fio-chronicle/config/config.ini ]]; then
    echo && echo "WARNING: A FIO.Chronicle configuration already exists!" && echo
    if yes_or_no "Overwrite /opt/fio-chronicle/config/config.ini"; then
      mv /opt/fio-chronicle/config/config.ini /opt/fio-chronicle/config/config.ini.$(date +%Y%m%d_%H%M%S)
    fi
  fi
  cp ${CONFIG_DIR}/config.ini.relic /opt/fio-chronicle/config/config.ini

  cp -r ${PROJECT_DIR}/testing /opt/fio-chronicle

  echo && echo "FIO.Chronicle has been successfully installed to /opt!"
else
  sudo cp ${BUILD_DIR}/chronicle-receiver /usr/local/sbin
  sudo cp ${PROJECT_DIR}/systemd/chronicle_receiver@.service /etc/systemd/system

  sudo mkdir -p /srv/fio/chronicle-data && sudo mkdir -p /srv/fio/chronicle-config

  if [[ -e /srv/fio/chronicle-config/config.ini ]]; then
    echo && echo "WARNING: A FIO.Chronicle configuration already exists!" && echo
    if yes_or_no "Overwrite /srv/fio/chronicle-config/config.ini"; then
      sudo mv /srv/fio/chronicle-config/config.ini /srv/fio/chronicle-config/config.ini.$(date +%Y%m%d_%H%M%S)
    fi
  fi
  sudo cp ${CONFIG_DIR}/config.ini.relic /srv/fio/chronicle-config/config.ini

  # Set up systemd

  echo && echo "FIO.Chronicle has been successfully installed to /usr/local/sbin!"
fi
echo
