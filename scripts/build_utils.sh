#!/usr/bin/env bash
set -x

pushdir() {
  DIR=$1
  #pushd ${DIR} &> /dev/null
  pushd ${DIR}
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
