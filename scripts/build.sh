#!/usr/bin/env bash

# Debug
#set -x

# Get Scripts dir and ensure we're in the repo root and not inside of scripts
SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";
cd $( dirname "${BASH_SOURCE[0]}" )/..

# Load utility functions
. ${SCRIPTS_DIR}/utils.sh

if [[ "$(uname)" == "Linux" ]]; then
  if [[ -e /etc/os-release ]]; then
    # obtain NAME and other information
    . /etc/os-release
    if [[ ${NAME} != "Ubuntu" ]]; then
      echo && echo "Currently only supporting Ubuntu based builds. Proceed at your own risk."
      pause
    fi
  else
    echo && echo "Currently only supporting Ubuntu based builds. /etc/os-release not found. Your Linux distribution is not supported. Proceed at your own risk."
    pause
  fi
else
  echo && echo "Currently only supporting Ubuntu based builds. Your architecture is not supported. Proceed at your own risk."
  pause
fi

if [[ $# -eq 0 || -z "$1" ]]; then
  echo
  echo "ERROR: No argument provided for Dependency directory" && echo
  echo "Usage:"
  echo "./scripts/build.sh DEPS_DIR"
  echo "  DEPS_DIR: directory to place build dependencies (Clang, LLVM, Boost)"
  echo
  exit -1
fi

if [[ ! -x "$1" ]]; then
  echo
  echo "ERROR: $1 is NOT valid; Check permissions and retry!" && echo
  echo "Usage:"
  echo "./scripts/build.sh DEPS_DIR"
  echo "  DEPS_DIR: directory to place build dependencies (Clang, LLVM, Boost)"
  echo
  exit -1
fi

groups $(id -un) | grep sudo >/dev/null
if [[ $? -ne 0 ]]; then
  echo
  echo "ERROR: User $(id -un) does NOT have sudo privilege! sudo privilege is required to run this script. Exiting..."
  echo
  exit 1
fi

DEPS_DIR=$1

echo && echo "Building Fio.Chronicle..."

ARCH=`uname -m`
JOBS=$(nproc)

CLANG_VER=11.0.1
BOOST_VER=1.80.0
LLVM_VER=7.1.0

HOME_DIR="$(pwd)"
BUILD_DIR=${HOME_DIR}/build

echo && echo "Installing build/run-time dependencies..."
sudo ${SCRIPTS_DIR}/install_deps.sh
sudo ${SCRIPTS_DIR}/install_pgsql.sh -d
. ${SCRIPTS_DIR}/build_deps.sh ${DEPS_DIR}

# build Chronicle
echo && echo "Building..."
makedir ${BUILD_DIR} && pushdir ${BUILD_DIR}
try cmake -DCMAKE_TOOLCHAIN_FILE=${SCRIPTS_DIR}/pinned_toolchain.cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=${LLVM_DIR}/lib/cmake -DCMAKE_PREFIX_PATH=${BOOST_DIR}/bin ${MORE_CMAKE_FLAGS} ${SCRIPTS_DIR}/..

try make -j${JOBS}
try cpack

if [[ ! -e ${BUILD_DIR}/chronicle-receiver ]]; then
  echo
  echo "ERROR: FIO.Chronicle has failed to build! Review build output for errors, correct and re-execute build script."
else
  echo
  echo "SUCCESS: FIO.Chronicle has been successfully built."
fi
