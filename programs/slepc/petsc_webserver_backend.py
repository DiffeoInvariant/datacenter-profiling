import os
import sys
from flask import Flask, request, Response
import re
import math
import time
import argparse
import jsonpickle

app = Flask(__name__)

entries = {}
entries_by_name = {}

datafile = '/opt/tcpsummary'

def read_file(filename):
    global entries
    global entries_by_name
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
            name = spl[1].rstrip('\n').strip(' ')
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
            val = [name,tx_kb,rx_kb,n_event,
                   avg_lat,avg_life,fraction_ipv6]

            if mpi_rank in entries:
                entries[mpi_rank][pid] = tuple(val)
            else:
                entries[mpi_rank] = {pid : tuple(val)}

            
            val += [mpi_rank,pid]
            if name in entries_by_name:
                entries_by_name[name].append(tuple(val))
            else:
                entries_by_name[name] = [tuple(val)]
            
            
        i += 1



def format_entry(entry,rank,pid):
        ipentry = f"{(100 * entry[6]):2.2f} " if entry[6] == 0.0 else f"{(100 * entry[6]):2.2f}"
        fstr = f"""
_________________________________________________________________
| Summary of network traffic on MPI rank {rank:3d} with PID {pid:8d}: |
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

def format_by_name(entry):
    mpi_rank, pid = entry[-2],entry[-1]
    return format_entry(entry,mpi_rank,pid)
                    

def format_entries():
    entry_ls = []
    for rk in entries:
        rk_entries = []
        for pid in entries[rk]:
            rk_entries.append(format_entry(entries[rk][pid],rk,pid))
        entry_ls.append('\n'.join(rk_entries))

    return '\n'.join(entry_ls)



        
@app.route('/')
def root():
    read_file(datafile)
    return format_entries()


def good_response(resp):
    return Response(response=jsonpickle.encode({'response' : resp}),status=200,mimetype='application/json')

@app.route('/api/get/all',methods=['GET'])
def get_all():
    read_file(datafile)
    resp = format_entries()
    return good_response(resp)


def key_not_found_response(key,dname):
    resp = f"Key {key} not found in dictionary {dname}!"
    return Response(response=jsonpickle.encode({'error' : resp}),status=404,mimetype='application/json')

@app.route('/api/get/<int:rank>/<int:pid>',methods=['GET'])
def get(rank,pid):
    read_file(datafile)
    try:
        entry = entries[rank]
    except KeyError:
        return key_not_found_response(rank,'entries')

    try:
        entry = entry[pid]
    except KeyError:
        return key_not_found_response(pid,f"entries[{rank}]")

    resp = format_entry(entry)
    return good_response(resp)




@app.route('/api/get/<string:name>',methods=['GET'])
def get_name(name):
    read_file(datafile)
    try:
        entry = entries_by_name[name]
    except KeyError:
        return key_not_found_response(name,'entries_by_name')
    resp = [format_by_name(e) for e in entry]
    return good_response(resp)
    

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
