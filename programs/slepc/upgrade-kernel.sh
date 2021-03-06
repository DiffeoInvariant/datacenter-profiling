#!/bin/sh

apt-get update && apt-get install -y wget

wget https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.10-rc6/amd64/linux-headers-5.10.0-051000rc6_5.10.0-051000rc6.202011291930_all.deb
wget https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.10-rc6/amd64/linux-headers-5.10.0-051000rc6-generic_5.10.0-051000rc6.202011291930_amd64.deb
wget https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.10-rc6/amd64/linux-image-unsigned-5.10.0-051000rc6-generic_5.10.0-051000rc6.202011291930_amd64.deb
wget https://kernel.ubuntu.com/~kernel-ppa/mainline/v5.10-rc6/amd64/linux-modules-5.10.0-051000rc6-generic_5.10.0-051000rc6.202011291930_amd64.deb
dpkg -i ./*.deb
