#!/bin/bash

usage() { echo "Usage: $0 [-n <int (num nodes)>] [-o <string (output file)]" 1>&2; exit 1; }

n=2
out=$HOME/tcpdata.log

while getopts ":n:o:" opt; do
    case "${opt}" in
	n)
	    n=${OPTARG}
	    ;;
	o)
	    out=${OPTARG}
	    ;;
    esac
done
shift $((OPTIND-1))
	    

mpirun -n ${n} --map-by node --hostfile hostfile ./webserver --accept_file $ACCEPT_DATA --connect_file $CONNECT_DATA --connlat_file $CONNLAT_DATA --life_file $LIFE_DATA
