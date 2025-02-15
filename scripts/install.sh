#!/usr/bin/env bash

# Debug
#set -x

echo && echo "Installing Fio.Chronicle..."

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

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

groups $(id -un) | grep sudo >/dev/null
if [[ $? -ne 0 ]]; then
  echo
  echo "ERROR: User $(id -un) does NOT have sudo privilege! sudo privilege is required to run this script. Exiting..."
  echo
  exit 1
fi

HOME_DIR=$(pwd)
BUILD_DIR=${HOME_DIR}/build
CONFIG_DIR=${HOME_DIR}/config

if [[ -e ${BUILD_DIR}/chronicle-receiver ]]; then
  makedir /opt/fio-chronicle
  makedir /opt/fio-chronicle/config
  makedir /opt/fio-chronicle/data

  cp ${BUILD_DIR}/chronicle-receiver /opt/fio-chronicle

  if [[ -e /opt/fio-chronicle/config/config.ini ]]; then
    echo
    echo "WARNING: A FIO.Chronicle configuration already exists!"
    echo
    if yes_or_no "Overwrite /opt/fio-chronicle/config/config.ini"; then
      mv /opt/fio-chronicle/config/config.ini /opt/fio-chronicle/config/config.ini.$(date +%Y%m%d_%H%M%S)
      cp ${CONFIG_DIR}/config.ini.sample /opt/fio-chronicle/config/config.ini
    fi
  else
    cp ${CONFIG_DIR}/config.ini.sample /opt/fio-chronicle/config/config.ini
  fi

  cp -r ${HOME_DIR}/testing /opt/fio-chronicle

  echo
  echo "FIO.Chronicle has been successfully installed to /opt/fio-chronicle."
else
  echo
  echo "ERROR: Unable to install FIO.Chronicle; ${BUILD_DIR}/chronicle-receiver does not exist!"
fi
echo
