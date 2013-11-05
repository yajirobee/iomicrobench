#! /usr/bin/env python

import sys, os, time
import subprocess as sp
import clearcache
from createworkload import create_workload

def main():
    prgdir = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
    outdir = "/data/local/keisuke/{0}".format(time.strftime("%Y%m%d%H%M%S", time.gmtime()))
    #os.mkdir(outdir)
    iodumpfile = "/tmp/iodump"
    nthreads = [1 << i for i in range(5)]
    for nthread in nthreads:
        create_workload(iodumpfile, 4000, nthread)
        clearcache.clear_cache(2 ** 30)
        p = sp.Popen([os.path.join(prgdir, "ioreplayer"), "-m", str(nthread), iodumpfile])
        p.wait()

if __name__ == "__main__":
    main()
