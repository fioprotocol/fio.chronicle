#!/usr/bin/env bash

echo
echo "Building Fio.Chronicle..."

if [[ "$(uname)" == "Linux" ]]; then
   if [[ -e /etc/os-release ]]; then
      # obtain NAME and other information
      . /etc/os-release
      if [[ ${NAME} != "Ubuntu" ]]; then
         echo "Currently only supporting Ubuntu based builds. Proceed at your own risk."
      fi
   else
       echo "Currently only supporting Ubuntu based builds. /etc/os-release not found. Your Linux distribution is not supported. Proceed at your own risk."
   fi
else
    echo "Currently only supporting Ubuntu based builds. Your architecture is not supported. Proceed at your own risk."
fi

if [[ $# -eq 0 || -z "$1" ]]; then
   echo "Usage:"
   echo "./scripts/build.sh DEPS_DIR"
   echo "  DEPS_DIR: directory where to place build dependencies (Clang, LLVM, Boost)"
   exit -1
fi

DEPS_DIR=$1

ARCH=`uname -m`
JOBS=$(nproc)

CLANG_VER=11.0.1
BOOST_VER=1.80.0
LLVM_VER=7.1.0

SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";

# Ensure we're in the repo root and not inside of scripts
cd $( dirname "${BASH_SOURCE[0]}" )/..

HOME_DIR="$(pwd)"
BUILD_DIR=${HOME_DIR}/build

. ${SCRIPTS_DIR}/build_utils.sh

echo
. ${SCRIPTS_DIR}/build_deps.sh ${DEPS_DIR}
echo

makedir ${BUILD_DIR} && pushdir ${BUILD_DIR}

# build Chronicle
echo "Building Chronicle"
try cmake -DCMAKE_TOOLCHAIN_FILE=${SCRIPTS_DIR}/pinned_toolchain.cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=${LLVM_DIR}/lib/cmake -DCMAKE_PREFIX_PATH=${BOOST_DIR}/bin ${MORE_CMAKE_FLAGS} ${SCRIPTS_DIR}/..

try make -j${JOBS}
try cpack

echo
echo "Chronicle has successfully built and constructed its packages.  You should be able to find the packages at ${BUILD_DIR}."
