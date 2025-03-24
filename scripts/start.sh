#!/usr/bin/env bash

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

function usage() {
   printf "\\nUsage: $0 OPTION...
  -b     FIO.Chronicle Binary Directory
  -d     FIO.Chronicle Data (State) Directory
  -r     Reset FIO.Chronicle state
  -x     Run in debub mode
  -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
RESET=${RESET:-false}
if [ $# -ne 0 ]; then
   while getopts "b:rxh" opt; do
      case "${opt}" in
      b)
         BIN_DIR=${OPTARG}
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

if [[ -z ${BIN_DIR} ]]; then
   BIN_DIR=/opt/fio-chronicle
   if [[ -x /opt/fio-chronicle/chronicle-receiver && -x /usr/local/sbin/chronicle-receiver ]]; then
      echo && echo "WARNING: FIO.Chronicle receiver found in /opt/fio-chronicle AND /usr/local/sbin!"
      echo && echo "Re-execute script providing '-b' arg. Exiting..."
      usage
   fi
fi

if [[ -n ${BIN_DIR} && ! ( -d ${BIN_DIR} && -x ${BIN_DIR}/chronicle-receiver ) ]]; then
   echo && echo "ERROR: FIO.Chronicle executable, ${BIN_DIR}/chronicle-receiver, invalid or not found!"
   usage
fi

PID=$(pgrep chronicle)
if [[ -n $PID ]]; then
   echo && echo "ERROR: FIO.Chronicle appears to be running! Use the stop script to stop FIO.Chronicle..."
   echo
   exit 1
fi

# Start Chronicle
echo && echo "Starting FIO.Chronicle..."
STARTED=false
if [[ -x /usr/local/sbin/chronicle-receiver ]]; then
   echo && echo "INFO: The FIO.Chronicle receiver appears to be installed as a service" & echo
   if yes_or_no "Should systemctl be used to start FIO.Chronicle receiver"; then
      sudo systemctl start chronicle-receiver
      STARTED=true
   fi
fi
if ! ${STARTED}; then
   ${BIN_DIR}/chronicle-receiver --config-dir=${BIN_DIR}/config --data-dir=${BIN_DIR}/data --end-block=400000000 2>&1 | tee -a ${BIN_DIR}/log/chronicle.log &
fi