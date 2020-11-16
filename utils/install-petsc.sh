#!/bin/sh

PETSC_BASE=${PETSC_BASE:-$HOME/datacenter-profiling}
CC=${CC:-clang}
CXX=${CXX:-clang++}
COPT=${COPTFLAGS:-"-O3 -g -march=native -mtune=native -fPIC"}
CXXOPT=${CXXOPTFLAGS:-"-O3 -g -march=native -mtune=native -fPIC"}
FOPT=${FOPTFLAGS:-"-O3 -g -march=native -mtune=native -fPIC"}
DEBUG={$DEBUG: -1}
mkdir -p $PETSC_BASE && cd $PETSC_BASE
git clone -b release https://gitlab.com/petsc/petsc.git petsc
cd petsc

CONF = --with-cc=$CC --with-cxx=$CXX COPTFLAGS=$COPT CXXOPTFLAGS=$CXXOPT FOPTFLAGS=$FOPT --download-mpich --download-saws --download-hwloc


if [ -n $DEBUG]; then
    CONF += --with-debugging=no
else
    CONF += --with-debugging=yes
fi

./configure $CONF
