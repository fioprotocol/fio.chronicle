#!/usr/bin/env bash

echo "Installing Fio.Chronicle..."

if [[ "$(uname)" == "Linux" ]]; then
   if [[ -e /etc/os-release ]]; then
      # obtain NAME and other information
      . /etc/os-release
      if [[ ${NAME} != "Ubuntu" ]]; then
         echo "Currently only supporting Ubuntu based builds. Proceed at your own risk."
      fi
   else
       echo "Currently only supporting Ubuntu based builds. /etc/os-release not found. Your Linux distribution is not supported. Proceed at your own risk."
   fi
else
   echo "Currently only supporting Ubuntu based builds. Your architecture is not supported. Proceed at your own risk."
fi

# Get scripts dir
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";

# Ensure we're in the repo root and not inside of scripts
cd $( dirname "${BASH_SOURCE[0]}" )/..

HOME_DIR=$(pwd)
BUILD_DIR=${HOME_DIR}/build
CONFIG_DIR=${HOME_DIR}/config

. ${SCRIPTS_DIR}/build_utils.sh

makedir /opt/fio-chronicle
makedir /opt/fio-chronicle
makedir /opt/fio-chronicle/config
makedir /opt/fio-chronicle/data

cp ${BUILD_DIR}/chronicle-receiver /opt/fio-chronicle
cp ${CONFIG_DIR}/config.ini.sample /opt/fio-chronicle/config/config.ini

#cp -r ${HOME_DIR}/testing /opt/fio-chronicle

echo "Chronicle has been successfully installed to /opt/fio-chronicle."
