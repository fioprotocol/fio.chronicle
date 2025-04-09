#!/usr/bin/env bash

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

# Test user perms
if [[ "$EUID" -eq 0 ]]; then
   echo && echo "ERROR: Script should not be run as root! Exiting..."
   echo
   exit 1
fi

# Perform initial verification
PID=$(pgrep chronicle)
if [[ -n $PID ]]; then
   echo && echo "ERROR: FIO.Chronicle appears to be running! Use the stop script to stop FIO.Chronicle..."
   echo
   exit 1
fi

function usage() {
   printf "\\nUsage: $0 OPTION...
   -b     FIO.Chronicle Binary Directory
   -d     FIO.Chronicle Data Directory
   -r     Reset FIO.Chronicle state
   -s     Service: Use systemctl to Start
   -x     Run in debug mode
   -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
RESET=${RESET:-false}
SERVICE=${SERVICE:-false}
if [ $# -ne 0 ]; then
   while getopts "b:d:rsxh" opt; do
      case "${opt}" in
      b)
         BIN_DIR=${OPTARG}
         ;;
      d)
         DATA_DIR=${OPTARG}
         ;;
      r)
         RESET=true
         ;;
      s)
         SERVICE=true
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

# Default install: /opt/fio-chronicle
# /opt/fio-chronicle/chronicle-receiver
# /opt/fio-chronicle/config/config.ini
# /opt/fio-chronicle/data/receiver-state
# /opt/fio-chronicle/log

# Service install
# /usr/local/sbin/chronicle-receiver
# /srv/fio/chronicle-config
# /srv/fio/chronicle-data
# /var/log?

echo && echo "Starting Fio.Chronicle..."
if ! ${SERVICE} ; then
   if systemctl -q is-active chronicle_receiver@fio && systemctl -q is-enabled chronicle_receiver@fio; then
      echo && echo "FIO.Chronicle receiver appears to be installed as a service"
      echo
      if yes_or_no "Use systemctl to start FIO.Chronicle receiver"; then
        SERVICE=true
      fi
   fi
fi

if ${SERVICE}; then
   groups $(id -un) | grep sudo >/dev/null
   if [[ $? -ne 0 ]]; then
      echo
      echo "ERROR: User $(id -un) does NOT have sudo privilege! sudo privilege is required to use systemctl. Exiting..."
      echo
      exit 1
   fi
fi

if ! ${SERVICE}; then
   BIN_DIR=${BIN_DIR:-/opt/fio-chronicle}
   if [[ ! ( -d ${BIN_DIR} && -x ${BIN_DIR}/chronicle-receiver ) ]]; then
      echo && echo "ERROR: FIO.Chronicle executable, ${BIN_DIR}/chronicle-receiver, invalid or not found!"
      usage
   fi
   DATA_DIR=${DATA_DIR:-${BIN_DIR}/data}
else
   DATA_DIR=${DATA_DIR:-/srv/fio/chronicle-data}
fi

if $RESET; then
   echo && echo "Reset FIO.Chronicle state..." && echo
   if yes_or_no "Proceed"; then
      sudo rm -f ${DATA_DIR}/receiver-state/lock.bin
      sudo rm -f ${DATA_DIR}/receiver-state/shared_memory.bin
  fi
fi

if ! ${SERVICE}; then
   [[ -e ${BIN_DIR}/log/chronicle.log ]] && mv ${BIN_DIR}/log/chronicle.log ${BIN_DIR}/log/chronicle-`date +%Y-%m-%dT%H%M%S`.log
   ${BIN_DIR}/chronicle-receiver --config-dir=${BIN_DIR}/config --data-dir=${BIN_DIR}/data --end-block=400000000 2>&1 | tee -a ${BIN_DIR}/log/chronicle.log &
else
   sudo systemctl start chronicle_receiver@fio
fi

sleep 1
PID=$(pgrep chronicle)
if [[ -z $PID ]]; then
   echo && echo "ERROR: FIO.Chonicle is not started!"
else
   echo && echo "FIO.Chonicle started!"
fi