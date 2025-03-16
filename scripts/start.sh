#!/usr/bin/env bash

function usage() {
   printf "Usage: $0 OPTION...
  -r      Reset FIO.Chronicle state
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
RESET=${RESET:-false}
SNAPSHOT=${SNAPSHOT:-false}
if [ $# -ne 0 ]; then
   while getopts "drs" opt; do
      case "${opt}" in
      d)
         DEBUG=true
         set -x
         ;;
      r)
         RESET=true
         ;;
      s)
         SNAPSHOT=true
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

echo && echo "Starting Fio.Chronicle..."

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

INSTALL_DIR=/opt/fio-chronicle

if [[ ! -d ${INSTALL_DIR} || ! -x ${INSTALL_DIR}/chronicle-receiver ]]; then
  echo && echo "ERROR: FIO.Chronicle binary, ${INSTALL_DIR}/chronicle-receiver, not found!"
  echo
  exit 1
fi

PID=$(pgrep chronicle)
if [[ -n $PID ]]; then
  echo && echo "ERROR: FIO.Chronicle appears to be running! Use the stop script to stop FIO.Chronicle..."
  echo
  exit 1
fi

if $RESET; then
  echo && echo "Reset FIO.Chronicle state..." && echo
  if yes_or_no "Proceed?"; then
    rm -f ${INSTALL_DIR}/data/receiver-state/lock.bin
    rm -f ${INSTALL_DIR}/data/receiver-state/shared_memory.bin
  fi
fi

if $SNAPSHOT; then
  echo && echo "Saving FIO.Chronicle snapshot..." && echo
  ${INSTALL_DIR}/chronicle-receiver --config-dir=${INSTALL_DIR}/config --data-dir=${INSTALL_DIR}/data --save-snapshot=${INSTALL_DIR}/bkups/fio-chronicle.snapshot-`date +%Y-%m-%dT%H%M%S`
fi
  
# Start Chronicle
echo && echo "Starting FIO.Chronicle..."
pause
makedir ${INSTALL_DIR}/log
[[ -e ${INSTALL_DIR}/log/chronicle.log ]] && mv ${INSTALL_DIR}/log/chronicle.log ${INSTALL_DIR}/log/chronicle-`date +%Y-%m-%dT%H%M%S`.log
${INSTALL_DIR}/chronicle-receiver --config-dir=${INSTALL_DIR}/config --data-dir=${INSTALL_DIR}/data --end-block=400000000 2>&1 | tee -a ${INSTALL_DIR}/log/chronicle.log &