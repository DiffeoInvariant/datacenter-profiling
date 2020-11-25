import requests
import json
import time
import argparse



addr_pre = 'http://'
addr = None

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
    
