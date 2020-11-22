FROM ubuntu:20.04
ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN apt-get update && apt-get install -y build-essential bison cmake flex git libedit-dev libllvm7 llvm-7-dev libclang-7-dev python zlib1g-dev python3-pip zlib1g-dev libelf-dev libfl-dev git netperf clang libclang-dev

RUN git clone https://github.com/iovisor/bcc.git

RUN mkdir -p bcc/build && cd bcc/build && cmake -DPYTHON_CMD=python3 .. && mkdir -p src/python && cd src/python && make && make install

#RUN pushd src/python && make && make install && popd

#download and install PETSc
RUN cd /usr/local && git clone -b release https://gitlab.com/petsc/petsc.git petsc

RUN cd /usr/local/petsc && ./configure PETSC_ARCH=arch-linux-c-opt --with-debugging=no --download-mpich --download-scalapack --download-slepc --download-f2cblaslapack=1 --with-cc=clang --with-cxx=clang++ COPTFLAGS="-O3 -g -march=native -mtune=native" CXXOPTFLAGS="-O3 -g -march=native -mtune=native" && make PETSC_DIR=/usr/local/petsc PETSC_ARCH=arch-linux-c-opt && make PETSC_DIR=/usr/local/petsc PETSC_ARCH=arch-linux-c-opt install

RUN export PETSC_DIR=/usr/local/petsc && export PETSC_ARCH=arch-linux-c-opt

RUN mkdir -p /programs && cd /programs

COPY Makefile /programs

COPY svd.c /programs

ENV n 4

RUN cd /programs && make svd

#User instructions: COPY your input datafile to a directory, set input_file to the correct value, and also set output_svals and output_svecs if you don't like the defaults
RUN mkdir -p /data
COPY ../../tests/bcsstk32.bin /data
COPY ../../tests/bcsstk32.bin.info /data
ENV input_file /data/bcsstk32.bin
ENV output_svals /data/svals.bin
ENV output_svecs /data/svecs.bin

CMD ["mpiexec -np ${n}", "/programs/svd", "-input ${input_file}", "-write_vals ${output_svals}", "-write_vecs ${output_svecs}"]

