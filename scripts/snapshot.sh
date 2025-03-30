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

PID=$(pgrep chronicle)
if [[ -n $PID ]]; then
   echo && echo "ERROR: FIO.Chronicle appears to be running! Stop FIO.Chronicle before proceeding."
   echo
   exit 1
fi

function usage() {
   printf "\\nUsage: $0 OPTION...
   -a     FIO.Chronicle Archive Directory/File
   -d     FIO.Chronicle Data Directory (state)
   -e     Export FIO.Chronicle State (Archive may be either a file or a directory)
   -i     Import FIO.Chronicle State (Archive must be a file)
   -s     Service: Start/Stop via systemctl
   -x     Run in debug mode
   -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
IMPORT=${IMPORT:-false}
EXPORT=${EXPORT:-false}
SERVICE=${SERVICE:-false}
if [ $# -ne 0 ]; then
   while getopts "a:d:eisxh" opt; do
      case "${opt}" in
      a)
         SNAPSHOT=${OPTARG}
         ;;
      d)
         DATA_DIR=${OPTARG}
         ;;
      e)
         EXPORT=true
         ACTION="Export"
         ;;
      i)
         IMPORT=true
         ACTION="Import"
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

if ! ( ${EXPORT} || ${IMPORT} ); then
   echo && echo "ERROR: Either export or import must be specified!"
   usage
fi

if ${IMPORT} && [[ -z $SNAPSHOT ]]; then
   echo && echo "ERROR: Import requires that a snapshot archive file is specified!"
   usage
fi
if ${IMPORT} && [[ -d ${SNAPSHOT} ]]; then
   echo && echo "ERROR: Import requires that a snapshot archive file is specified!"
   usage
fi
if $IMPORT && [[ ! -r ${SNAPSHOT} ]]; then
   echo && echo "ERROR: Unable to read snapshot archive file ${SNAPSHOT}!"
   usage
fi

if ! ${SERVICE} ; then
   if systemctl -q is-enabled chronicle_receiver@fio; then
      echo && echo "FIO.Chronicle receiver appears to be installed as a service"
      echo
      if yes_or_no "Is FIO.Chronicle receiver installed as a service"; then
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
   DATA_DIR=${DATA_DIR:-/opt/fio-chronicle/data}
   BKUP_DIR=${BKUP_DIR:-/opt/fio-chronicle/bkup}
   if [[ ! -d ${DATA_DIR} ]]; then
      echo && echo "ERROR: FIO.Chronicle data directory, ${DATA_DIR}, invalid or not found!"
      usage
   fi
else
   DATA_DIR=${DATA_DIR:-/srv/fio/chronicle-data}
   BKUP_DIR=${BKUP_DIR:-/srv/fio/chronicle-bkup}
fi

if [[ -d ${SNAPSHOT} ]]; then
   SNAPSHOT=${SNAPSHOT}/fc-snapshot_`date +%Y-%m-%dT%H%M%S`.tar.gz
fi
SNAPSHOT=${SNAPSHOT:-${BKUP_DIR}/fc-snapshot_`date +%Y-%m-%dT%H%M%S`.tar.gz}
echo
if ! yes_or_no "$ACTION data to: ${SNAPSHOT}"; then
   usage
fi

SNAPSHOT_DIR=${SNAPSHOT%/*}
makedir ${SNAPSHOT_DIR}
if $EXPORT && [[ ! -w ${SNAPSHOT_DIR} ]]; then
   echo && echo "Unable to write snapshot archive to ${SNAPSHOT_DIR}!"
   usage
fi

if $EXPORT; then
   echo && echo "Exporting FIO.Chronicle snapshot to..."
   # EOS Chronicle
   #${INSTALL_DIR}/chronicle-receiver --config-dir=${INSTALL_DIR}/config --data-dir=${INSTALL_DIR}/data --save-snapshot=${INSTALL_DIR}/bkup/fio-chronicle.snapshot-`date +%Y-%m-%dT%H%M%S`
   tar -czf ${SNAPSHOT} -C ${DATA_DIR}/receiver-state lock.bin shared_memory.bin
fi

if $IMPORT; then
   echo && echo "Importing FIO.Chronicle snapshot..."
   # EOS Chronicle
   #${INSTALL_DIR}/chronicle-receiver --config-dir=${INSTALL_DIR}/config --data-dir=${INSTALL_DIR}/data --save-snapshot=${INSTALL_DIR}/bkup/fio-chronicle.snapshot-`date +%Y-%m-%dT%H%M%S`
   tar -xzf ${SNAPSHOT} -C ${DATA_DIR}/receiver-state
fi
echo && echo "Finished"