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
  -s     FIO.Chronicle Snapshot Directory/File
  -x     Run in debug mode
  -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
IMPORT=${IMPORT:-false}
EXPORT=${EXPORT:-false}
if [ $# -ne 0 ]; then
   while getopts "eis:xh" opt; do
      case "${opt}" in
      e)
         EXPORT=true
         ACTION="Export"
         ;;
      i)
         IMPORT=true
         ACTION="Import"
         ;;
      s)
         SNAPSHOT=${OPTARG}
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
   echo && echo "ERROR: ${INSTALL_DIR}/chronicle-receiver not found!"
   usage
fi

if ! ( ${EXPORT} || ${IMPORT} ); then
   echo && echo "ERROR: Either export or import must be specified!"
   usage

fi

if ${IMPORT} && [[ -z $SNAPSHOT ]]; then
   echo && echo "ERROR: Import requires a snapshot is specified!"
   usage
fi

if [[ -n ${SNAPSHOT} ]]; then
   if [[ -d ${SNAPSHOT} ]]; then
      SNAPSHOT=${SNAPSHOT}/fc-snapshot_`date +%Y-%m-%dT%H%M%S`.tar.gz
   fi
fi
SNAPSHOT=${SNAPSHOT:-${INSTALL_DIR}/bkup/fc-snapshot_`date +%Y-%m-%dT%H%M%S`.tar.gz}
echo
if ! yes_or_no "Confirm: $ACTION data to: ${SNAPSHOT}"; then
   usage
fi

SNAPSHOT_DIR=${SNAPSHOT%/*}
makedir ${SNAPSHOT_DIR}
if $EXPORT && [[ ! -w ${SNAPSHOT_DIR} ]]; then
   echo && echo "Unable to write snapshot to ${SNAPSHOT_DIR}!"
   usage
fi
if $IMPORT && [[ ! -r ${SNAPSHOT} ]]; then
   echo && echo "Unable to read snapshot from ${SNAPSHOT}!"
   usage
fi

PID=$(pgrep chronicle)
if [[ -n $PID ]]; then
   echo && echo "ERROR: FIO.Chronicle appears to be running! Stop FIO.Chronicle, using the stop script 'stop.sh', before proceeding."
   echo
   exit 1
fi

if $EXPORT; then
   echo && echo "Exporting FIO.Chronicle snapshot..." && echo
   # EOS Chronicle
   #${INSTALL_DIR}/chronicle-receiver --config-dir=${INSTALL_DIR}/config --data-dir=${INSTALL_DIR}/data --save-snapshot=${INSTALL_DIR}/bkup/fio-chronicle.snapshot-`date +%Y-%m-%dT%H%M%S`
   tar -czf ${SNAPSHOT} -C ${INSTALL_DIR}/data/receiver-state lock.bin shared_memory.bin
fi

if $IMPORT; then
   echo && echo "Importing FIO.Chronicle snapshot..." && echo
   # EOS Chronicle
   #${INSTALL_DIR}/chronicle-receiver --config-dir=${INSTALL_DIR}/config --data-dir=${INSTALL_DIR}/data --save-snapshot=${INSTALL_DIR}/bkup/fio-chronicle.snapshot-`date +%Y-%m-%dT%H%M%S`
   tar -xzf ${SNAPSHOT} -C ${INSTALL_DIR}/data/receiver-state
fi
