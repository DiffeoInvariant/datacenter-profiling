#!/bin/bash

usage() {echo "Usage: $0 [-d <cluster-deployment-name>] [-n <cluster-name>] [-r <cluster-region>] [-z <cluster-zone>]" 1>&2; exit 1;}

while getopts ":d:n:r:z:" o; do
    case "${o}" in
	d)
	    dname=${OPTARG}
	    ;;
	n)
	    cname=${OPTARG}
	    ;;
	r)
	    reg=${OPTARG}
	    ;;
	z)
	    zone=${OPTARG}
	    ;;
	*)
	    usage
	    ;;
    esac
done
shift $((OPTIND-1))

if [ -z "${d}" ] || [ -z "${n}" ] || [ -z "${r}" ] || [ -z "${z}" ]; then
    usage
fi

git clone https://github.com/SchedMD/slurm-gcp.git || true

export CLUSTER_DEPLOY_NAME="${d}"
export CLUSTER_NAME="${n}"
export CLUSTER_REGION="${r}"
export CLUSTER_ZONE="${z}"

cd slurm-gcp && cp slurm-cluster.yaml ${CLUSTER_DEPLOY_NAME}.yaml

