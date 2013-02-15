#! /usr/bin/env python

import sys, os, subprocess, shlex, re, sqlite3, itertools, time
import iobench

iomaxes = [2 ** i for i in range(20, 36)]
iosize = 2 * 13
thread = 1
iterate = 100000
parameters = (([iosize], [iterate], [thread], iomaxes),)
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
    outdir = "/data/local/keisuke/{0}".format(
        time.strftime("%Y%m%d%H%M%S", time.gmtime()))
    os.mkdir(outdir)

    fpath = sys.argv[1]
    bname = os.path.splitext(os.path.basename(fpath))[0]
    dbpath = "{0}/readspec_{1}.db".format(outdir, bname)
    rbench = iobench.readbenchmarker(fpath, outdir)
    rbench.varnames.append("fsize")
    rbench.setcmdtemplate("{0} {1} {{0}} {{1}} {{2}} {{3}}")

    # random read
    sys.stdout.write("random read\n")
    rbench.setcmd("./randomread")
    randrecorder = iobench.iobenchrecorder(dbpath, "random_read",
                                           rbench.varnames, rbench.resnames,
                                           rbench.run)
    for p in parameters:
        randrecorder.allmeasure(itertools.product(*p))
