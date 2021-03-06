#!/bin/bash

mkdir -p $HOME/tcpaccept_data
mkdir -p $HOME/tcpconnect_data
mkdir -p $HOME/tcplife_data
mkdir -p $HOME/tcpconnlat_data
mkdir -p $HOME/tcpretrans_data

apt-get update && apt-get install -y wget build-essential bison cmake flex git libedit-dev libllvm7 llvm-7-dev libclang-7-dev python python3.8 python3.8-dev zlib1g-dev python3-pip zlib1g-dev libelf-dev libfl-dev git netperf gfortran pkg-config valgrind unzip linux-headers-generic nfs-kernel-server




pip3 install --upgrade flask jsonpickle
echo "Installing BCC..."
git clone https://github.com/iovisor/bcc.git
mkdir -p bcc/build && cd bcc/build && cmake .. && make && make install && cmake -DPYTHON_CMD=python3 .. && pushd src/python && cd src/python && make && make install && popd
make install

 export OMPI_URL="https://download.open-mpi.org/release/open-mpi/v4.0/openmpi-4.0.0.tar.bz2"

 mkdir -p /tmp/ompi
    
 cd /tmp/ompi && wget -O openmpi-4.0.0.tar.bz2 $OMPI_URL && tar -xjf openmpi-4.0.0.tar.bz2
 cd /tmp/ompi/openmpi-4.0.0 && ./configure --prefix=/opt/ompi && make -j 24 install
 export PATH=/opt/ompi/bin:$PATH
 export LD_LIBRARY_PATH=$OMPI_DIR/lib:$LD_LIBRARY_PATH
 export MANPATH=$OMPI_DIR/share/man:$MANPATH

echo "Installing PETSc and SLEPc..."
cd /usr/local && git clone -b release https://gitlab.com/petsc/petsc.git petsc
cd /usr/local/petsc && ./configure PETSC_ARCH=arch-linux-c-opt --with-debugging=yes --download-scalapack --download-slepc --download-f2cblaslapack=1 COPTFLAGS="-O3 -g -march=native -mtune=native" CXXOPTFLAGS="-O3 -g -march=native -mtune=native" FOPTFLAGS="-O3 -g -march=native -mtune=native" --with-mpi-dir=/opt/ompi
make PETSC_DIR=/usr/local/petsc PETSC_ARCH=arch-linux-c-opt -C /usr/local/petsc -j 24
export PETSC_DIR=/usr/local/petsc && export PETSC_ARCH=arch-linux-c-opt
    
echo "Compiling svd program..."
make PETSC_DIR=/usr/local/petsc PETSC_ARCH=arch-linux-c-opt -C /opt svd

echo "Compiling PETSc webserver..."
make PETSC_DIR=/usr/local/petsc PETSC_ARCH=arch-linux-c-opt -C /opt webserver
make PETSC_DIR=/usr/local/petsc PETSC_ARCH=arch-linux-c-opt -C /opt webserver_launcher
