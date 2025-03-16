#!/usr/bin/env bash

function usage() {
   printf "Usage: $0 OPTION...
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
SNAPSHOT=${SNAPSHOT:-false}
if [ $# -ne 0 ]; then
   while getopts "ds" opt; do
      case "${opt}" in
      d)
         DEBUG=true
         set -x
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

echo && echo "Stopping Fio.Chronicle..."

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

INSTALL_DIR=/opt/fio-chronicle
  
# Stop chronicle (note this depends on an idle postgres)
echo && echo "Stopping FIO.Chronicle gracefully..."
COUNTER=0
while true; do
   let COUNTER++
   PID=$(pgrep chronicle)
   if [[ -z $cr_pid ]]; then
     echo && echo "FIO.Chronicle is stopped."
     break
   fi

   if [[ -n $PID ]]; then
      ps -ef | grep -v grep | grep relicdb | grep -q idle && kill -INT $PID && echo && echo " Stopped!"
   fi

   if [[ COUNTER > 200 ]]; then
     echo && echo "WARNING: Unable to shut down gracefully, therefore, just shutting down..."
     pause
     kill -INT $PID
     break
   fi
done

if $SNAPSHOT; then
  echo && echo "Saving FIO.Chronicle snapshot..." && echo
  makedir ${INSTALL_DIR}/bkups
  # EOS Chronicle
  #${INSTALL_DIR}/chronicle-receiver --config-dir=${INSTALL_DIR}/config --data-dir=${INSTALL_DIR}/data --save-snapshot=${INSTALL_DIR}/bkups/fio-chronicle.snapshot-`date +%Y-%m-%dT%H%M%S`
  tar -czf ${INSTALL_DIR}/bkups/fc-snapshot-`date +%Y-%m-%dT%H%M%S`.tar.gz -C ${INSTALL_DIR}/data/receiver-state .
fi
echo