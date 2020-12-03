#!/usr/bin/python3
from petsc4py import PETSc
import scipy.io
import urllib.request
import tempfile
from contextlib import closing
import shutil
import gzip
import argparse
import logging
import sys

def convert_matrix(matrix_market_filename: str, petsc_binary_filename: str):
    scipy_mat = scipy.io.mmread(gzip.open(matrix_market_filename,'rb'))
    scipy_mat = scipy_mat.tocsr()
    petsc_mat = PETSc.Mat().createAIJ(size=scipy_mat.shape,
                                      csr=(scipy_mat.indptr,scipy_mat.indices,
                                           scipy_mat.data),
                                      )
    viewer = PETSc.Viewer().createBinary(petsc_binary_filename,'w')
    viewer(petsc_mat)



def convert_matrix_market_file(url, convert_to):
    gzfile = tempfile.NamedTemporaryFile()
    with closing(urllib.request.urlopen(url)) as response:
        gzfile.seek(0)
        zipped_matrix = response.read()
        gzfile.write(zipped_matrix)

    convert_matrix(gzfile.name,convert_to)



if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Get a file from the Matrix Market (in Matrix Market format) and convert it to a PETSc matrix')
    parser.add_argument('urls',type=str,nargs='+',help='URLs to get the matrices from')
    parser.add_argument('-o','--output',type=str,nargs='+',default='petscmat.bin',help='Names of the binary files you want the matrices written to')
    parser.add_argument('-q','--quiet',action='store_true',help='Only print warnings (no info)')
    args = parser.parse_args()
    last_valid_outid = len(args.output)
    last_valid_outname = args.output[-1]
    last_valid_suffix = None
    if '.' in last_valid_outname:
        parts = last_valid_outname.split('.')
        last_valid_outname = parts[0]
        last_valid_suffix = '.'.join(parts[1:])
    overflow_id = 0
    if len(args.urls) > len(args.output):
        logging.warning(f'matrix-market-converter: {len(args.urls)} URLs given but only {len(output)} output filenames given. Appending increasing numbers to the last valid output filename for subsequent URLs')
        
    for i,url in enumerate(args.urls):
        if i >= last_valid_outid:
            overflow_id += 1
            outname = last_valid_outname + '_' + str(overflow_id) + last_valid_suffix
        else:
            outname = args.output[i]

        try:
            if not args.quiet:
                print(f"Downloading {url} and placing the result in {outname}")
            convert_matrix_market_file(url,outname)
        except KeyboardInterrupt:
            raise
        except Exception as exc:
            logging.warning(f"matrix-market-converter: caught exception {exc} while trying to download and convert a matrix from {url}.")
