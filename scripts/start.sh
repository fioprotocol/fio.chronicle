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
  -x     Turn debug on
  -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
RESET=${RESET:-false}
if [ $# -ne 0 ]; then
   while getopts "b:d:l:rxh" opt; do
      case "${opt}" in
      b)
         BIN_DIR=${OPTARG}
         ;;
      d)
         DATA_DIR=${OPTARG}
         ;;
      l)
         LOG_DIR=${OPTARG}
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

# sudo cp ${BUILD_DIR}/chronicle-receiver /usr/local/sbin
# sudo mkdir -p /srv/fio/chronicle-data && sudo mkdir -p /srv/fio/chronicle-config

BIN_DIR=${BIN_DIR:-/opt/fio-chronicle}
if [[ ! -z ${BIN_DIR} ]]; then
   echo && echo "ERROR: ${BIN_DIR}/chronicle-receiver not found! Use '-b' to specify the FIO.Chronicle binary directory."
   usage
   exit 1
fi

if [[ -n ${BIN_DIR} && ! ( -d ${BIN_DIR} && -x ${BIN_DIR}/chronicle-receiver ) ]]; then
   echo && echo "ERROR: FIO.Chronicle binary, ${BIN_DIR}/chronicle-receiver, not found!"
   echo
   exit 1
fi

SYSTEMCTL_INUSE=false
if [[ -x /usr/local/sbin/chronicle-receiver ]]; then
   echo && echo "INFO: The FIO.Chronicle receiver appears to be installed as a service"
   echo "      Proceeding will use systemctl to start FIO.Chronicle receiver..."
   pause
   BIN_DIR=/usr/local/sbin/chronicle-receiver
   SYSTEMCTL_INUSE=true
   if [[ $RESET ]]; then
     echo && echo "WARNING: RESET is not possible when using systemctl; Reset state manually..."
     pause
   fi
   RESET=false
fi

echo && echo "Starting Fio.Chronicle..."

PID=$(pgrep chronicle)
if [[ -n $PID ]]; then
   echo && echo "ERROR: FIO.Chronicle appears to be running! Use the stop script to stop FIO.Chronicle..."
   echo
   exit 1
fi

if $RESET; then
   echo && echo "Reset FIO.Chronicle state..." && echo
   if yes_or_no "Proceed"; then
      rm -f ${DATA_DIR}/lock.bin
      rm -f ${DATA_DIR}/shared_memory.bin
  fi
fi

# Start Chronicle
echo && echo "Starting FIO.Chronicle..."
if $SYSTEMCTL_INUSE; then
   sudo systemctl start chronicle-receiver
else
   if [[ ${BIN_DIR}/log ]]; then
      [[ -e ${BIN_DIR}/log/chronicle.log ]] && mv ${BIN_DIR}/log/chronicle.log ${BIN_DIR}/log/chronicle-`date +%Y-%m-%dT%H%M%S`.log
      ${BIN_DIR}/chronicle-receiver --config-dir=${BIN_DIR}/config --data-dir=${BIN_DIR}/data --end-block=400000000 2>&1 | tee -a ${BIN_DIR}/log/chronicle.log &
   fi
fi