#!/usr/bin/env bash

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

function usage() {
   printf "\\nUsage: $0 OPTION...
  -x     Run in debub mode
  -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

DEBUG=${DEBUG:-false}
if [ $# -ne 0 ]; then
   while getopts "xh" opt; do
      case "${opt}" in
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
echo && echo -n "Stopping FIO.Chronicle gracefully..."
COUNTER=0
while [ $COUNTER -lt 100 ]; do
   COUNTER=$(($COUNTER+1))

   PID=$(pgrep chronicle)
   if [[ -n $PID ]]; then
      ps -ef | grep -v grep | grep relicdb | grep -q idle && kill -INT $PID
   else
      echo "stopped!"
      break
   fi
done

# Check if really down...
PID=$(pgrep chronicle)
if [[ -n $PID ]]; then
   echo && echo "WARNING: Unable to shut down gracefully! Shut down by force?"
   pause
   kill -INT $PID
fi

sleep 1
PID=$(pgrep chronicle)
if [[ -n $PID ]]; then
   echo && echo "ERROR: FIO.Chonicle not shut down!"
else
   echo && echo "FIO.Chronicle shut down"
fi