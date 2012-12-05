#! /usr/bin/env python

import sys, os, subprocess, shlex, re, sqlite3, itertools, time
import iobench

outdir = "/data/local/keisuke/{0}".format(
    time.strftime("%Y%m%d%H%M%S", time.gmtime()))
os.mkdir(outdir)

iomax = 2 ** 37
parameters = ([2 ** i for i in range(9, 22)], [10000], [1])
'''
nthreads = [2 ** i for i in range(11)]
parameters = (([2 ** 9], [2 ** 15], nthreads),
              ([2 ** 13], [2 ** 12], nthreads),
              ([2 ** 16], [2 ** 10], nthreads))
'''

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stdout.write("Usage : {0} fpath\n".format(sys.argv[0]))
        sys.exit(0)
    fpath = sys.argv[1]
    bname = os.path.splitext(os.path.basename(fpath))[0]
    dbpath = "{0}/readspec_{1}.db".format(outdir, bname)
    rbench = iobench.readbenchmarker(fpath, outdir)

    # sequential read
    sys.stdout.write("sequential read\n")
    rbench.setcmd("./sequentialread")
    seqrecorder = iobench.iobenchrecorder(dbpath, "sequential_read",
                                          rbench.varnames, rbench.resnames,
                                          rbench.run)
    for p in parameters:
        iosizes, iterates, nthreads = p
        assert max(iosizes) * max(iterates) * max(nthreads) <= iomax, "excessive io size"
        seqrecorder.allmeasure(itertools.product(iosizes, iterates, nthreads))

    # random read
    sys.stdout.write("random read\n")
    rbench.setcmd("./randomread")
    randrecorder = iobench.iobenchrecorder(dbpath, "random_read",
                                           rbench.varnames, rbench.resnames,
                                           rbench.run)
    for p in parameters:
        iosizes, iterates, nthreads = p
        assert max(iosizes) * max(iterates) * max(nthreads) <= iomax, "excessive io size"
        randrecorder.allmeasure(itertools.product(iosizes, iterates, nthreads))
