#!/usr/bin/env bash

# Debug
#set -x

DEPS_DIR=$1
if [[ -z $DEPS_DIR ]]; then
  echo "ERROR: Unable to proceed; no directory provided to place build dependencies"
  echo
  exit 1
fi

ARCH=`uname -m`
JOBS=$(nproc)

BOOST_VER=1.80.0
CLANG_VER=11.0.1
CMAKE_VER=3.31.2
LLVM_VER=7.1.0

SCRIPTS_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}"; )" &> /dev/null && pwd 2> /dev/null; )";

# Ensure we're in the repo root and not inside of scripts
cd $( dirname "${BASH_SOURCE[0]}" )/..

HOME_DIR="$(pwd)"
BUILD_DIR=${HOME_DIR}/build

. ${SCRIPTS_DIR}/build_utils.sh

echo && echo "Checking package dependencies (pre-built)..."
sudo ${SCRIPTS_DIR}/install_deps.sh
echo Done

install_cmake() {
  CMAKE_DIR=$1
  if [[ ! -d "${CMAKE_DIR}" ]]; then
    echo "Installing Cmake ${CMAKE_VER} @ ${CMAKE_DIR}"
    if [[ ${ARCH} = x86_64 ]]; then
      CMAKE_FN=cmake-${CMAKE_VER}-linux-x86_64
      INSTALL_SCRIPT=cmake-${CMAKE_VER}-linux-x86_64.sh
    elif [[ ${ARCH} = aarch64 ]]; then
      CMAKE_FN=cmake-${CMAKE_VER}-linux-aarch64
      INSTALL_SCRIPT=cmake-${CMAKE_VER}-linux-aarch64.sh
    else
      echo "Unknown ARCH: $ARCH"
      exit 1
    fi

    pushdir /tmp
    try wget https://github.com/Kitware/CMake/releases/download/v${CMAKE_VER}/${INSTALL_SCRIPT}
    chmod +x ${INSTALL_SCRIPT}
    sudo sh ${INSTALL_SCRIPT} --prefix=/tmp --include-subdir --skip-license
    rm -f ${INSTALL_SCRIPT}
    sudo mv /tmp/${CMAKE_FN} /opt/cmake-${CMAKE_VER}
    popdir ${DEPS_DIR}
  fi
  
  echo $PATH | grep '/usr/local/bin' >/dev/null
  if [[ $? ]]; then
    echo "/usr/local/bin is on PATH; cmake may be found at /usr/local/bin/cmake"
    if [[ ! -x /usr/local/bin/cmake ]]; then
      sudo ln -s /opt/cmake-${CMAKE_VER}/bin/cmake /usr/local/bin/cmake
    fi
  else
    echo "usr/local/bin is NOT on PATH; cmake may be found at /opt/cmake-${CMAKE_VER}/bin/cmake"
    export PATH="/opt/cmake-${CMAKE_VER}/bin:$PATH"
  fi
}

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

echo && echo "Checking build dependencies (cmake, clang, llvm, boost)..."
pushdir ${DEPS_DIR}
install_cmake ${DEPS_DIR}/cmake-${CMAKE_VER}
install_clang ${DEPS_DIR}/clang-${CLANG_VER}
install_llvm ${DEPS_DIR}/llvm-${LLVM_VER}
install_boost ${DEPS_DIR}/boost_${BOOST_VER//\./_}
echo Done
