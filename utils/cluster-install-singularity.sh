#!/bin/bash

export CLUSTER_LOGIN_NODE=$(gcloud compute instances list \
			    --zones $CLUSTER_ZONE \
			    --filter="name ~ .*login." \
			    --format="value(name)" | head -n1)\
				gcloud compute ssh ${CLUSTER_LOGIN_NODE} --zone $CLUSTER_ZONE


sudo yum update -y && \
    sudo yum groupinstall -y 'Development Tools' && \
    sudo yum install -y \
	 openssl-devel \
	 libuuid-devel \
	 libseccomp-devel \
	 wget \
	 squashfs-tools \
	 cryptsetup


export GOLANG_VERSION=1.15.5
export OS=linux ARCH=amd64
wget https://dl.google.com/go/go$GOLANG_VERSION.$OS-$ARCH.tar.gz
sudo tar -C /usr/local -xzvf go$GOLANG_VERSION.$OS-$ARCH.tar.gz
rm go$GOLANG_VERSION.$OS-$ARCH.tar.gz

echo 'export GOPATH=${HOME}/go' >> ~/.bashrc
echo 'export PATH=/usr/local/go/bin:${PATH}:${GOPATH}/bin' >> ~/.bashrc
source ~/.bashrc

export SINGULARITY_VERSION=3.6.4
wget https://github.com/sylabs/singularity/releases/download/v${SINGULARITY_VERSION}/singularity-${SINGULARITY_VERSION}.tar.gz && \
    tar -xzf singularity-${SINGULARITY_VERSION}.tar.gz && \
    cd singularity

./mconfig --prefix=/apps/singularity/${SINGULARITY_VERSION} && \
    make -C ./builddir && \
    sudo make -C ./builddir install


sudo mkdir -p /apps/modulefiles/singularity
sudo bash -c "cat > /apps/modulefiles/singularity/${SINGULARITY_VERSION}" << SINGULARITY_MODULEFILE
#%Module1.0#####################################################################
##
## modules singularity/${SINGULARITY_VERSION}.
##
## modulefiles/singularity/${SINGULARITY_VERSION}.
##
proc ModulesHelp { } {
     global version modroot
      puts stderr "singularity/${SINGULARITY_VERSION} - sets the environment for Singularity ${SINGULARITY_VERSION}"
}

module-whatis "Sets the environment for Singularity ${VERSION}"

set  topdir /apps/singularity/${SINGULARITY_VERSION}
set  version ${SINGULARITY_VERSION}
set  sys     linux86
prepend-path PATH \$topdir/bin
SINGULARITY_MODULEFILE
