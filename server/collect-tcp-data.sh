#!/bin/sh

mkdir -p $HOME/tcpaccept_data
mkdir -p $HOME/tcpcollect_data
mkdir -p $HOME/tcpconnlat_data
mkdir -p $HOME/tcplife_data
mkdir -p $HOME/tcpretrans_data

/usr/share/bcc/tools/tcpaccept > $HOME/tcpaccept_data/tcpaccept.log &
/usr/share/bcc/tools/tcpconnect > $HOME/tcpconnect_data/tcpconnect.log &
/usr/share/bcc/tools/tcpconnlat > $HOME/tcpconnlat_data/tcpconnlat.log &
/usr/share/bcc/tools/tcplife > $HOME/tcplife_data/tcplife.log &
/usr/share/bcc/tools/tcpretrans > $HOME/tcpretrans_data/tcpretrans.log &
