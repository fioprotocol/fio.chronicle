#!/usr/bin/env bash

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

function usage() {
   printf "\\nUsage: $0 OPTION...
  -i     FIO.Chronicle Install Directory
  -r     Reset FIO.Chronicle state
  -x     Run in debub mode
  -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
RESET=${RESET:-false}
if [ $# -ne 0 ]; then
   while getopts "i:rxh" opt; do
      case "${opt}" in
      i)
         INSTALL_DIR=${OPTARG}
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

if [[ -n ${INSTALL_DIR} && ! ( -d ${INSTALL_DIR} && -x ${INSTALL_DIR}/chronicle-receiver ) ]]; then
   echo && echo "ERROR: FIO.Chronicle executable, ${INSTALL_DIR}/chronicle-receiver, invalid or not found!"
   usage
fi

PID=$(pgrep chronicle)
if [[ -n $PID ]]; then
   echo && echo "ERROR: FIO.Chronicle appears to be running! Use the stop script to stop FIO.Chronicle..."
   echo
   exit 1
fi

echo && echo "Starting Fio.Chronicle..."
IS_SERVICE=false
if [[ -z ${INSTALL_DIR} ]]; then
   if systemctl -q is-active chronicle-receiver; then
      echo && echo "INFO: The FIO.Chronicle receiver appears to be installed as a service"
      echo && echo "Using systemctl to start FIO.Chronicle receiver..."; then
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

INSTALL_DIR=${INSTALL_DIR:-/opt/fio-chronicle}
[[ -e ${INSTALL_DIR}/log/chronicle.log ]] && mv ${INSTALL_DIR}/log/chronicle.log ${INSTALL_DIR}/log/chronicle-`date +%Y-%m-%dT%H%M%S`.log
${INSTALL_DIR}/chronicle-receiver --config-dir=${INSTALL_DIR}/config --data-dir=${INSTALL_DIR}/data --end-block=400000000 2>&1 | tee -a ${INSTALL_DIR}/log/chronicle.log &
