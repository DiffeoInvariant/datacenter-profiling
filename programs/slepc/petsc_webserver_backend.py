import os
import sys
from flask import Flask, request
import re
import math
import time
import argparse

app = Flask(__name__)

entries = {}

datafile = '/opt/tcpsummary'

def read_file(filename):
    lines = open(filename,'r').readlines()
    lines_per_entry = 7 #7 data fields and a header
    N = len(lines)
    i = 0
    while i < N:
        if lines[i].startswith('Summary of network traffic on rank '):
            if i + lines_per_entry >= N:
                print(f"Error, found early-terminated entry starting with header {lines[i]}")
                break #start of an entry, but no end; something went wrong
            nums = list(map(int,re.findall(r'\d+',lines[i])))
            mpi_rank, pid = nums
            i += 2 # skip pid line
            spl = lines[i].split('= ')
            name = spl[1].rstrip('\n')
            i += 1
            spl = lines[i].split('= ')
            tx_kb = int(spl[1])
            i += 1
            spl = lines[i].split('= ')
            rx_kb = int(spl[1])
            i += 1
            spl = lines[i].split('= ')
            n_event = int(spl[1])
            i += 1
            spl = lines[i].split('= ')
            try:
                avg_lat = float(spl[1])
            except ValueError:
                avg_lat = math.nan

            i += 1
            spl = lines[i].split('= ')
            try:
                avg_life = float(spl[1])
            except ValueError:
                avg_life = math.nan

            i += 1
            spl = lines[i].split('= ')
            fraction_ipv6 = float(spl[1])

            if mpi_rank in entries:
                entries[mpi_rank][pid] = (name,tx_kb,rx_kb,n_event,
                                          avg_lat,avg_life,fraction_ipv6)
            else:
                entries[mpi_rank] = {pid : (name,tx_kb,rx_kb,n_event,
                                            avg_lat,avg_life,fraction_ipv6)}
        i += 1



def format_entries():
    def format_entry(entry):
        ipentry = f"{(100 * entry[6]):2.2f} " if entry[6] == 0.0 else f"{(100 * entry[6]):2.2f}"
        fstr = f"""
_________________________________________________________________
| Summary of network traffic on MPI rank {rk:3d} with PID {pid:8d}: |
|        process name............... = {entry[0]:15s}          |
|        transmitted kB............. = {entry[1]:8d}                 |
|        received kB................ = {entry[2]:8d}                 |
|        num TCP events............. = {entry[3]:8d}                 |
|        Average connection latency  = {entry[4]:8.2f}                 |
|        Average connection lifetime = {entry[5]:8.2f}                 |
|        Percent IPv6............... = {ipentry}                    |
|_______________________________________________________________|
        """
        return fstr

    entry_ls = []
    for rk in entries:
        rk_entries = []
        #print(f"entries[{rk}] = {entries[rk]}")
        for pid in entries[rk]:
            rk_entries.append(format_entry(entries[rk][pid]))
        entry_ls.append('\n'.join(rk_entries))

    return '\n'.join(entry_ls)



        
@app.route('/')
def root():
    read_file(datafile)
    return format_entries()
    
    

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-f','--file',default='/opt/tcpsummary',help='Where is the PETSc webserver (program webserver) dumping its output to?')
    parser.add_argument('-p','--port',type=int,default=5000,help='Which port to run Flask on?')
    parser.add_argument('--no_flask',action='store_true')
    args = parser.parse_args()
    datafile = args.file
    if args.no_flask:
        read_file(datafile)
        print(format_entries())
    else:
        app.run(host='0.0.0.0',port=args.port)
