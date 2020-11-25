import os
import sys
from flask import Flask, request
import re
import math

app = Flask(__name__)

entries = {}

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

#@app.route('/')
#def 



read_file('example.out')

print(f"entries is \n{entries}")
