#!/usr/bin/env bash
set -x

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

sudo ${SCRIPTS_DIR}/install_deps.sh

install_clang() {
  CLANG_DIR=$1
  if [[ ! -d "${CLANG_DIR}" ]]; then
    echo "Installing Clang ${CLANG_VER} @ ${CLANG_DIR}"
    makedir ${CLANG_DIR}
    if [[ ${ARCH} = x86_64 ]]; then
      CLANG_FN=clang+llvm-${CLANG_VER}-x86_64-linux-gnu-ubuntu-16.04.tar.xz
    elif [[ ${ARCH} = aarch64 ]]; then
      CLANG_FN=clang+llvm-${CLANG_VER}-aarch64-linux-gnu.tar.xz
    else
      echo "Unknown ARCH: $ARCH"
      exit 1
    fi

    pushdir /tmp
    try wget https://github.com/llvm/llvm-project/releases/download/llvmorg-${CLANG_VER}/${CLANG_FN}
    try tar -xvf ${CLANG_FN}
    pushdir ${CLANG_DIR}
    mv /tmp/clang+*/* .
    popdir /tmp
    rm -rf ${CLANG_FN}
    popdir ${DEPS_DIR}
  fi
  export PATH=${CLANG_DIR}/bin:$PATH
  export CLANG_DIR=${CLANG_DIR}
}

install_llvm() {
  LLVM_DIR=$1
  if [[ ! -d "${LLVM_DIR}" ]]; then
    echo "Installing LLVM ${LLVM_VER} @ ${LLVM_DIR}"
    makedir ${LLVM_DIR}
    pushdir /tmp
    try wget https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VER}/llvm-${LLVM_VER}.src.tar.xz
    try tar -xvf llvm-${LLVM_VER}.src.tar.xz
    pushdir "llvm-${LLVM_VER}.src"
    makedir build && pushdir build
    try cmake -DCMAKE_TOOLCHAIN_FILE=${SCRIPTS_DIR}/pinned_toolchain.cmake -DCMAKE_INSTALL_PREFIX=${LLVM_DIR} -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=host -DLLVM_BUILD_TOOLS=Off -DLLVM_ENABLE_RTTI=On -DLLVM_ENABLE_TERMINFO=Off -DCMAKE_EXE_LINKER_FLAGS=-pthread -DCMAKE_SHARED_LINKER_FLAGS=-pthread -DLLVM_ENABLE_PIC=NO ..
    try make -j${JOBS}
    try make -j${JOBS} install
    popdir "/tmp/llvm-${LLVM_VER}.src"
    popdir /tmp
    rm -rf llvm-${LLVM_DIR}.src
    rm llvm-${LLVM_VER}.src.tar.xz
    popdir ${DEPS_DIR}
  fi
  export LLVM_DIR=${LLVM_DIR}
}

install_boost() {
  BOOST_DIR=$1
  if [[ ! -d "${BOOST_DIR}" ]]; then
    echo "Installing Boost ${BOOST_VER} @ ${BOOST_DIR}"
    makedir ${BOOST_DIR}
    pushdir /tmp
    try wget https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VER}/source/boost_${BOOST_VER//\./_}.tar.gz
    try tar -xvzf boost_${BOOST_VER//\./_}.tar.gz
    mv boost_${BOOST_VER//\./_}/* ${BOOST_DIR}
    pushdir ${BOOST_DIR}
    try ./bootstrap.sh -with-toolset=clang --prefix=${BOOST_DIR}/bin
    ./b2 toolset=clang cxxflags='-stdlib=libc++ -D__STRICT_ANSI__ -nostdinc++ -I${CLANG_DIR}/include/c++/v1 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE' linkflags='-stdlib=libc++ -pie' link=static threading=multi --with-iostreams --with-date_time --with-filesystem --with-system --with-program_options --with-chrono --with-test --with-thread -q -j${JOBS} install
    popdir /tmp
    rm boost_${BOOST_VER//\./_}.tar.gz
    popdir ${DEPS_DIR}
  fi
  export BOOST_DIR=${BOOST_DIR}
}

pushdir ${DEPS_DIR}

install_clang ${DEPS_DIR}/clang-${CLANG_VER}
install_llvm ${DEPS_DIR}/llvm-${LLVM_VER}
install_boost ${DEPS_DIR}/boost_${BOOST_VER//\./_}
