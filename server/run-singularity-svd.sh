#!/bin/bash

usage() { echo "Usage: $0 [-n <int (num processors)>] [-c <string (Singularity container name)>] [-p <string (program name within container)>] [-i <string (input file)] [-v <string (singular vecs output file)>] [-s <string (singular values output file)>]" 1>&2; exit 1; }

while getopts ":n:p:v:s:" o; do
    case "${o}" in
	c)
	    c=${OPTARG}
	    ;;
	n)
	    n=${OPTARG}
	   ((n > 0)) || usage
	   ;;
	p)
	    p=${OPTARG}
	    ;;
	i)
	    i=${OPTARG}
	    ;;
	v)
	    v=${OPTARG}
	    ;;
	s)
	    s=${OPTARG}
	    ;;
	*)
	    usage
	    ;;
    esac
done
shift $((OPTIND-1))

if [ -z "${s}" ] || [ -z "${p}" ] || [ -z "${i}" ] || [ -z "${n}" ] || [ -z "${v}" ] [ -z "${c}" ]; then
    usage
fi


mpiexec -n ${n} singularity exec ${c} ${p} -input ${i} -write_vals ${s} -write_vecs ${v}


