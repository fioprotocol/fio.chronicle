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

function usage() {
   printf "\\nUsage: $0 OPTION...
   -s     Service: Use systemctl to Stop
   -x     Run in debug mode
   -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
SERVICE=${SERVICE:-false}
if [ $# -ne 0 ]; then
   while getopts "sxh" opt; do
      case "${opt}" in
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

# Stop chronicle (note this depends on an idle postgres)
echo && echo "Stopping FIO.Chronicle gracefully..."

if ! ${SERVICE} ; then
   if systemctl -q is-active chronicle_receiver@fio && systemctl -q is-enabled chronicle_receiver@fio; then
      echo && echo "FIO.Chronicle receiver appears to be installed and enabled as a service"
      echo
      if yes_or_no "Use systemctl to stop FIO.Chronicle receiver"; then
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
   PID=$(pgrep chronicle)

   if ! ps -ef | grep -v grep | grep -q postgresql; then
      kill -INT $PID
   else
      COUNTER=0
      while [ $COUNTER -lt 100 ]; do
         COUNTER=$(($COUNTER+1))

         PID=$(pgrep chronicle)
         if [[ -n $PID ]]; then
            ps -ef | grep relicdb | grep -v grep | grep -q idle && kill -INT $PID
         else
            break
         fi
      done
   fi

   # Check if really down...
   sleep 1
   PID=$(pgrep chronicle)
   if [[ -n $PID ]]; then
      echo && echo "WARNING: Unable to shut down gracefully! Shut down by force?"
      pause
      kill -INT $PID
   fi
else
   sudo systemctl stop chronicle_receiver@fio
fi

sleep 1
PID=$(pgrep chronicle)
if [[ -n $PID ]]; then
   echo && echo "ERROR: FIO.Chonicle is not stopped!"
else
   echo && echo "FIO.Chonicle stopped!"
fi