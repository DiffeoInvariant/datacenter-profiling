#!/bin/sh

PETSC_BASE=${PETSC_BASE:-$HOME/datacenter-profiling}
CC=clang
CXX=clang++
COPT='-O3 -g -march=native -mtune=native -fPIC'
CXXOPT='-O3 -g -march=native -mtune=native -fPIC'
FOPT='-O3 -g -march=native -mtune=native -fPIC'
DEFAULT_DBG=1
DEBUG={$DEBUG: -$DEFAULT_DBG}
mkdir -p $PETSC_BASE && cd $PETSC_BASE
git clone -b release https://gitlab.com/petsc/petsc.git petsc
export PETSC_DIR=$(pwd)/petsc
cd petsc

CONF="--with-cc=${CC} --with-cxx=${CXX} COPTFLAGS=${COPT} CXXOPTFLAGS=${CXXOPT} FOPTFLAGS=${FOPT} --download-mpich --download-saws --download-hwloc"
echo "Conf is "
echo $CONF
export PETSC_ARCH=arch-linux-c

if [ -n $DEBUG]; then
    export PETSC_ARCH=arch-linux-c-opt
    $CONF +=--with-debugging=no
else
    export PETSC_ARCH=arch-linux-c-debug
    $CONF +=--with-debugging=yes
fi

env PETSC_DIR=$PETSC_DIR PETSC_ARCH=$PETSC_ARCH ./configure $CONF
make PETSC_DIR=$PETSC_DIR PETSC_ARCH=$PETSC_ARCH all
