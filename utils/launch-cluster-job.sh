#!/bin/bash

usage() { echo "Usage $0: [-c COMMAND]" 1>&2; exit 1; }

while getopts ":c:" o; do
    case "${o}" in
	c)
	    cmd=${OPTARG}
	    ;;
	*)
	    usage
	    ;;
    esac
done

shift $((OPTIND-1))

if [-z "${cmd}"]; then
    usage
fi

gcloud compute ssh ${CLUSTER_NAME}-login1 --command '${cmd}' --zone $CLUSTER_ZONE
