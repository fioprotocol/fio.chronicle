#!/usr/bin/env bash

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

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
   -x     Run in debug mode
   -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
RESET=${RESET:-false}
if [ $# -ne 0 ]; then
   while getopts "b:d:rxh" opt; do
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

# System install
# /usr/local/sbin/chronicle-receiver
# /srv/fio/chronicle-config
# /srv/fio/chronicle-data
# /var/log?

if [[ -n ${BIN_DIR} && ! ( -d ${BIN_DIR} && -x ${BIN_DIR}/chronicle-receiver ) ]]; then
   echo && echo "ERROR: FIO.Chronicle executable, ${BIN_DIR}/chronicle-receiver, invalid or not found!"
   usage
fi

echo && echo "Starting Fio.Chronicle..."
IS_SERVICE=false
if [[ -z ${BIN_DIR} ]]; then
   if systemctl -q is-active chronicle-receiver; then
      echo && echo "FIO.Chronicle receiver appears to be installed as a service"
      echo && echo "Using systemctl to start FIO.Chronicle receiver..."
      pause
      if [[ $RESET ]]; then
         echo && echo "WARNING: RESET is not possible when using systemctl; State must be reset manually..."
         pause
      fi
      sudo systemctl start chronicle-receiver
      exit 0
   fi
fi

if $RESET; then
   echo && echo "Reset FIO.Chronicle state..." && echo
   if yes_or_no "Proceed"; then
      rm -f ${DATA_DIR}/lock.bin
      rm -f ${DATA_DIR}/shared_memory.bin
  fi
fi

BIN_DIR=${BIN_DIR:-/opt/fio-chronicle}
[[ -e ${BIN_DIR}/log/chronicle.log ]] && mv ${BIN_DIR}/log/chronicle.log ${BIN_DIR}/log/chronicle-`date +%Y-%m-%dT%H%M%S`.log
${BIN_DIR}/chronicle-receiver --config-dir=${BIN_DIR}/config --data-dir=${BIN_DIR}/data --end-block=400000000 2>&1 | tee -a ${BIN_DIR}/log/chronicle.log &

echo && echo "Finished"