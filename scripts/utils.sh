#!/usr/bin/env bash

# Debug
#set -x

# May be called two ways
# if yes_or_no "echo if"; then echo "if"; else echo "else"; fi
# if [[ $(yes_or_no "if test") -eq 0 ]]; then echo "if"; else echo "else"; fi
function yes_or_no {
  while true; do
    read -p "$*? [y/n]: " yn
    case $yn in
      [Yy]*) return 0;;
      [Nn]*) echo "Skipping..."; return  1;;
      * ) echo Invalid response entered! Try again...;;
    esac
  done
}

# May be called two ways
# if exit_on "Yes"; then echo "exiting"; else echo "not exiting"; fi
# if [[ $(exit_on "Yes") -eq 0 ]]; then echo "exiting"; else echo "not exiting"; fi
function exit_on {
  while true; do
    read -p "$*? [y/n]: " yn
    case $yn in
      [Yy]*) exit 0;;
      [Nn]*) echo "Continuing..."; return 0;;
      * ) echo Invalid response entered! Try again...;;
    esac
  done
}

function pause(){
  echo
  read -s -n 1 -p "Press any key to continue (CTRL-c to exit)..."
  echo
  echo
}

function wait_on {
  echo "Press any key to continue"
  while [ true ]; do
    read -t 3 -n 1
    if [[ $? = 0 ]]; then
      break;
    fi
  done
}

pushdir() {
  DIR=$1
  #pushd ${DIR}
  pushd ${DIR} &> /dev/null
}

popdir() {
  EXPECTED=$1
  D=`popd`
  popd &> /dev/null
  echo ${D}
  D=`eval echo $D | head -n1 | cut -d " " -f1`

  if [[ ${D} != ${EXPECTED} ]]; then
    echo "Directory is not where expected EXPECTED=${EXPECTED} at ${D}"
    exit 1
  fi
}

perm_error="Permission denied"
makedir() {
  DIR=$1
  made="$(mkdir -p $DIR 2>&1 >/dev/null)"
  if [[ $made =~ $perm_error ]]; then
    sudo mkdir -p $DIR;
    sudo chown ubuntu:ubuntu $DIR
  elif [[ $made -ne 0 ]]; then
    echo "Unable to make directory, $DIR, exiting..."
    exit 1
  fi
}

try(){
  output=$($@)
  res=$?
  if [[ ${res} -ne 0 ]]; then
    exit -1
  fi
}
