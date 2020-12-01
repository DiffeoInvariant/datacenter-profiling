#!/bin/bash

mkdir -p /tcpaccept_data
touch /tcpaccept_data/tcpaccept.log
mkdir -p /tcpconnect_data
mkdir -p /tcplife_data
mkdir -p /tcpconnlat_data
mkdir -p /tcpretrans_data
mkdir -p /bin
export TZ=America/Los_Angeles && \
    ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
echo "Installing dependencies..."

yum update -y && yum install -y gcc gcc-c++ make cmake wget bison flex git libedit-dev llvm-toolset-7 python python38 python38-devel zlib1g-devel python3-pip libelf-devel libfl-devel netperf gfortran pkg-config valgrind unzip linux-headers-generiv

pip3 install --upgrade flask jsonpickle
echo "Installing BCC..."
git clone https://github.com/iovisor/bcc.git
mkdir -p bcc/build && cd bcc/build && cmake .. && make && make install && cmake -DPYTHON_CMD=python3 .. && mkdir -p src/python && cd src/python && make && make install
make -C /bcc/build install
    
echo "Installing PETSc and SLEPc..."
cd /usr/local && git clone -b release https://gitlab.com/petsc/petsc.git petsc && pwd && ls
cd /usr/local/petsc && ./configure PETSC_ARCH=arch-linux-c-opt --with-debugging=yes --download-scalapack --download-slepc --download-f2cblaslapack=1 COPTFLAGS="-O3 -g -march=native -mtune=native" CXXOPTFLAGS="-O3 -g -march=native -mtune=native" FOPTFLAGS="-O3 -g -march=native -mtune=native" --download-mpich
make PETSC_DIR=/usr/local/petsc PETSC_ARCH=arch-linux-c-opt -C /usr/local/petsc
export PETSC_DIR=/usr/local/petsc && export PETSC_ARCH=arch-linux-c-opt
    
echo "Compiling svd program..."
make PETSC_DIR=/usr/local/petsc PETSC_ARCH=arch-linux-c-opt -C /opt svd

echo "Compiling PETSc webserver..."
make PETSC_DIR=/usr/local/petsc PETSC_ARCH=arch-linux-c-opt -C /opt webserver
make PETSC_DIR=/usr/local/petsc PETSC_ARCH=arch-linux-c-opt -C /opt webserver_launcher
