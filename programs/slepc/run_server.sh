#!/bin/sh

$MPIEXEC -n $NNODE --map-by ppr:1:node ./webserver --accept_file $HOME/tcpaccept_data/tcpaccept.log --connect_file $HOME/tcpconnect_data/tcpconnect.log --connlat_file $HOME/tcpconnlat_data/tcpconnlat.log --life_file $HOME/tcplife_data/tcplife.log --retrans_file $HOME/tcpretrans_data/tcpretrans.log -o $HOME/tcpdata.log
