#!/usr/bin/env bash

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

# Perform initial verification
# Test OS
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

# Test user perms
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

function usage() {
   echo
   printf "Usage: $0 OPTION...
   -i     Intall Directory (FIO.Chronicle binary). Default: /opt/fio-chronicle
   -s     System Install: /usr/local/sbin and /srv/fio, Start/Stop via systemctl
   -x     Run in debug mode
   -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

# Set global vars and get command line options
DEBUG=${DEBUG:-false}
SYSTEM_INSTALL=${SYSTEM_INSTALL:-false}
if [ $# -ne 0 ]; then
   while getopts "i:sxh" opt; do
      # echo "flag -$flag, Argument $OPTARG";
      case "${opt}" in
      i)
         INSTALL_DIR=$OPTARG
         if [[ z"${OPTARG:0:1}" == "z-" ]]; then
            echo "Install Dir, ${INSTALL_DIR}, is invalid!"
            exit 2
         fi
         ;;
      s)
         SYSTEM_INSTALL=true
         ;;
      x)
         DEBUG=true
         set -x
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

# Test user passing both -i and -s
if ${SYSTEM_INSTALL} && [[ -n ${INSTALL_DIR} ]]; then
   echo && echo "WARNING: Ignoring ${INSTALL_DIR} (-i) due to System Install (-s)"
   pause
fi

# Set Install dir to default if not set
INSTALL_DIR=${INSTALL_DIR:-/opt/fio-chronicle}

echo && echo "Installing Fio.Chronicle..."
if ${SYSTEM_INSTALL}; then
   echo "   Type             : System Install"
   echo "   Install Directory: /usr/local/sbin"
   echo "   Config Directory : /srv/fio/chronicle-config"
   echo "   Data Directory   : /srv/fio/chronicle-data"
else
   echo "   Type:              LocaL Install"
   echo "   Install Directory: ${INSTALL_DIR}"
   echo "   Config Directory : ${INSTALL_DIR}/config"
   echo "   Data Directory   : ${INSTALL_DIR}/data"
fi
echo
if ! yes_or_no "Proceed"; then
   usage
fi

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
echo
if yes_or_no "Clean up any existing install"; then
   if $SYSTEM_INSTALL; then
      sudo systemctl stop chronicle_receiver@fio
      sudo systemctl disable chronicle_receiver@fio
      sudo systemctl daemon-reload
      sudo rm -f /usr/local/sbin/chronicle-receiver && sudo rm -rf /srv/fio/ \
         && sudo rm -f /lib/systemd/system/chronicle_receiver@.service
   else
      rm -rf ${INSTALL_DIR}
   fi
fi

if ! $SYSTEM_INSTALL; then
   makedir ${INSTALL_DIR}
   makedir ${INSTALL_DIR}/config
   makedir ${INSTALL_DIR}/data
   makedir ${INSTALL_DIR}/log

   cp ${BUILD_DIR}/chronicle-receiver ${INSTALL_DIR}

   if [[ -e ${INSTALL_DIR}/config/config.ini ]]; then
      echo && echo "WARNING: A FIO.Chronicle configuration already exists!" && echo
      if yes_or_no "Overwrite ${INSTALL_DIR}/config/config.ini"; then
         mv ${INSTALL_DIR}/config/config.ini ${INSTALL_DIR}/config/config.ini.$(date +%Y%m%d_%H%M%S)
      fi
   fi
   cp ${CONFIG_DIR}/config.ini.relic ${INSTALL_DIR}/config/config.ini

   cp -r ${PROJECT_DIR}/testing ${INSTALL_DIR}

   echo && echo "FIO.Chronicle has been successfully installed to /opt!"
else
   sudo mkdir -p /srv/fio/chronicle-data && sudo mkdir -p /srv/fio/chronicle-config
   if [[ -e /srv/fio/chronicle-config/config.ini ]]; then
      echo && echo "WARNING: A FIO.Chronicle configuration already exists!" && echo
      if yes_or_no "Overwrite /srv/fio/chronicle-config/config.ini"; then
         sudo mv /srv/fio/chronicle-config/config.ini /srv/fio/chronicle-config/config.ini.$(date +%Y%m%d_%H%M%S)
      fi
   fi
   sudo cp ${CONFIG_DIR}/config.ini.relic /srv/fio/chronicle-config/config.ini

   sudo cp ${BUILD_DIR}/chronicle-receiver /usr/local/sbin
   # sudo cp ${PROJECT_DIR}/systemd/chronicle_receiver\@.service /etc/systemd/system/
   sudo cp ${PROJECT_DIR}/systemd/chronicle_receiver\@.service /lib/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable chronicle_receiver@fio

   echo && echo "FIO.Chronicle has been successfully installed to /usr/local/sbin!"

   echo && echo "Please verify configuration: /srv/fio/chronicle-config/config.ini"
   echo && echo "Use systemctl to start and stop FIO.Chronicle (using sudo if necessary)"
   echo "   systemctl start chronicle_receiver@fio"
   echo "   systemctl stop chronicle_receiver@fio"
   echo && echo "Use journalctl to tail the FIO.Chronicle log"
   echo "   journalctl -u chronicle_receiver@fio -f"
fi
echo
