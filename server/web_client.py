import requests
import json
import time
import argparse
import curses



addr_pre = 'http://'
addr = addr_pre + 'localhost:5000'

def make_addr(host,port):
    global addr
    addr = addr_pre + host
    if port is not None:
        addr += ':' + str(port)

        
def get_all():
    response = requests.get(addr + '/api/get/all')
    response = json.loads(response.text)
    if 'error' in response:
        print(f"Error on request to GET {addr + '/api/get/all'}:\n{response['error']}")
        return

    return response['response']
    


def get_one(rank, pid):
    response = requests.get(addr + f'/api/get/{rank}/{pid}')
    response = json.loads(response.text)
    if 'error' in response:
        print(f"Error on request to GET {addr + '/api/get/{rank}/{pid}'}:\n{response['error']}")
        return

    return response['response']



def get_name(name):
    response = requests.get(addr + f'/api/get/{name}')
    response = json.loads(response.text)
    if 'error' in response:
        print(f"Error on request to GET {addr + '/api/get/{name}'}:\n{response['error']}")

    return response['response']


    
if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-H','--host',default='localhost',help='Web host')
    parser.add_argument('-p','--port',default=5000,type=int,help='Which TCP port is the server using?')
    parser.add_argument('-n','--name',default='svd',help='Name of program you want to get information about')
    args = parser.parse_args()
    make_addr(args.host,args.port)
    response = get_all()
    print(f"get_all() gave\n{response}")
    response = get_name(args.name)
    print(f"There are {len(response)} entries with name {args.name}.")
    print(f"get_name({args.name}) returns:")
    for r in response:
        print(r)
