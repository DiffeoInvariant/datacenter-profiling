#!/bin/bash
#SBATCH -n 4


module load gcc/8.2.0
module load singularity/3.6.4
module load openmpi/4.0.0

singularity pull --name svd.sif shub://DiffeoInvariant/datacenter-profiling

N=4

mpirun -np ${N} --map-by ppr:1:node singularity exec svd.sif /opt/webserver \
       --accept_file /tcpaccept_data/tcpaccept.log \
       --connect_file /tcpconnect_data/tcpconnect.log \
       --connlat_file /tcpconnlat_data/tcpconnlat.log \
       --life_file /tcplife_data/tcplife.log \
       --retrans_file /tcpretrans_data/tcpretrans.log \
       -o /opt/tcpdata.log

mpirun -np ${N} --map-by ppr:1:node singularity exec svd.sif /opt/svd -input /opt/bcsstk32.bin -write_vals /opt/svals.bin -write_vecs /opt/svecs.bin
