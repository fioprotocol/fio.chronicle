#!/usr/bin/env bash

# Debug
#set -x

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

cr_pid=$(pgrep chronicle)
if [[ -n $cr_pid ]]; then
  echo && echo "WARNING: FIO.Chronicle appears to be running! "
  echo
  if yes_or_no "Stop FIO.Chronicle"; then
    kill -INT ${cr_pid}
  else
    echo && echo "FIO.Chronicle must be stopped before proceeding. Exiting..."
    echo
    exit 1
  fi
fi

sleep 1
cr_pid=$(pgrep chronicle)
if [[ -n $cr_pid ]]; then
  echo && echo "ERROR: FIO.Chronicle is running! Exiting..."
  echo
  exit 1
fi

echo
if yes_or_no "Reset FIO.Chronicle state"; then
  rm -f ${INSTALL_DIR}/data/receiver-state/lock.bin
  rm -f ${INSTALL_DIR}/data/receiver-state/shared_memory.bin
fi  
  
# Start Chronicle
echo && echo "Starting FIO.Chronicle..."
pause
makedir ${INSTALL_DIR}/log
${INSTALL_DIR}/chronicle-receiver --config-dir=${INSTALL_DIR}/config --data-dir=${INSTALL_DIR}/data --end-block=400000000 2>&1 | tee -a ${INSTALL_DIR}/log/chronicle.log &