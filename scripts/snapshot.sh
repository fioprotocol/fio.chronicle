#!/usr/bin/env bash

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

function usage() {
   printf "\\nUsage: $0 OPTION...
  -e     Export FIO.Chronicle State
  -i     Import FIO.Chronicle State
  -s     FIO.Chronicle Snapshot Directory
  -x     Run in debub mode
  -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
IMPORT=${IMPORT:-false}
EXPORT=${EXPORT:-false}
DEFAULT_SNAPSHOT_DIR=/opt/fio-chronicle/bkups
if [ $# -ne 0 ]; then
   while getopts "eis:xh" opt; do
      case "${opt}" in
      e)
         EXPORT=true
         ;;
      i)
         IMPORT=true
         ;;
      s)
         SNAPSHOT_DIR=${OPTARG}
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

INSTALL_DIR=${INSTALL_DIR:-/opt/fio-chronicle}
if [[ ! -e ${INSTALL_DIR}/chronicle-receiver ]]; then
   echo && echo "ERROR: ${INSTALL_DIR}/chronicle-receiver not found! Use '-i' to specify the FIO.Chronicle binary directory."
   usage
   exit 1
fi

if ! (${EXPORT} || ${IMPORT}); then
   echo && echo "ERROR: Either export or import must be specified!"
   usage

fi
if ${EXPORT} || ${IMPORT}; then
   if [[ -z ${SNAPSHOT_DIR} || ! -d ${SNAPSHOT_DIR} ]]; then
   echo && echo "WARNING: A valid snapshot directory was not specified ('-s'). Using default directory: ${DEFAULT_SNAPSHOT_DIR}"
   pause
   SNAPSHOT_DIR=${DEFAULT_SNAPSHOT_DIR}
fi
makedir ${SNAPSHOT_DIR}

PID=$(pgrep chronicle)
if [[ -n $PID ]]; then
   echo && echo "ERROR: FIO.Chronicle appears to be running! Stop FIO.Chronicle, using the stop script 'stop.sh', before proceeding."
   echo
   exit 1
fi

if $EXPORT; then
   echo && echo "Saving FIO.Chronicle snapshot..." && echo
   # EOS Chronicle
   #${INSTALL_DIR}/chronicle-receiver --config-dir=${INSTALL_DIR}/config --data-dir=${INSTALL_DIR}/data --save-snapshot=${INSTALL_DIR}/bkups/fio-chronicle.snapshot-`date +%Y-%m-%dT%H%M%S`
   tar -czf ${SNAPSHOT_DIR}/fc-snapshot_`date +%Y-%m-%dT%H%M%S`.tar.gz ${DATA_DIR}/receiver-state
fi

if $IMPORT; then
   echo && echo "Loading FIO.Chronicle snapshot..." && echo
   # EOS Chronicle
   #${INSTALL_DIR}/chronicle-receiver --config-dir=${INSTALL_DIR}/config --data-dir=${INSTALL_DIR}/data --save-snapshot=${INSTALL_DIR}/bkups/fio-chronicle.snapshot-`date +%Y-%m-%dT%H%M%S`
   tar -xzf ${SNAPSHOT_DIR}/fc-snapshot_`date +%Y-%m-%dT%H%M%S`.tar.gz -C ${BIN_DIR}/data/receiver-state
fi
