#!/bin/sh

PETSC_DIR=$(pwd)/petsc
PETSC_ARCH=arch-linux-c-opt
curl -sL https://slepc.upv.es/download/distrib/slepc-3.14.0.tar.gz | tar zx
export SLEPC_DIR=$(pwd)/slepc-3.14.0
SLEPC_DIR=$(pwd)/slepc-3.14.0
cd $SLEPC_DIR
./configure --download-arpack
make SLEPC_DIR=$SLEPC_DIR PETSC_DIR=$PETSC_DIR
