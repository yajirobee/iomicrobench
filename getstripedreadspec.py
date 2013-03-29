#! /usr/bin/env python

import sys, os, re, shlex, subprocess, time, sqlite3, itertools
import iobench

class stripedreadbenchmarker(iobench.readbenchmarker):
    timepat = re.compile(r"(stime|ftime|elapsed|latency) = (\d+(?:\.\d*)?)(?:\(us\))?")
    mbpspat = re.compile(r"(mbps) = (\d+(?:\.\d*)?)\(MB/s\)")
    iopspat = re.compile(r"(iops) = (\d+(?:\.\d*)?)\(io/s\)")
    patterns = (timepat, mbpspat, iopspat)

    resnames = ["elapsed", "mbps", "iops", "latency", "fullutil"]

    def __init__(self, outdir, perfflg = False, statflg = False):
        self.outdir = ourdir
        self.perfflg = perfflg
        self.statflg = statflg

    def run(self, fpaths, iosize, iterate, nthread):
        sys.stdout.write(("storage benchmark started\n"
                          "nlu = {nlu}, iosize = {iosize}, "
                          "iterate = {iterate}, nthread = {nthread}\n"
                          .format(nlu = len(fpaths), iosize = iosize,
                                  iterate = iterate, nthread = nthread)))
        ppool = []
        # for fpath in fpaths:
        #     ppool.append(subprocess.Popen(shlex.split(
        #                 self.cmd.format(fpath = fpath, iosize = iosize,
        #                                 iterate = iterate, nthread = nth)),
        #                                   stdout = subprocess.PIPE))
        nthreadlist = [nthread / len(fpaths) for lu in fpaths]
        for i in range(nthread % len(fpaths)):
            nthreadlist[i] += 1
        if self.perfflg:
            perfpath = ("{0}/stripe_io{1}_thr{2}_lu{3}.perf"
                        .format(self.outdir, iosize, nthread, len(fpaths)))
            self.cmd = "perf record -a -o {0} ".format(perfpath) + self.cmd
        if self.statflg:
            ioprofpath = ("{0}/stripe_io{1}_thr{2}_lu{3}.io"
                          .format(self.outdir, iosize, nthread, len(fpaths)))
            cpuprofpath = ("{0}/stripe_io{1}_thr{2}_lu{3}.cpu"
                           .format(self.outdir, iosize, nthread, len(fpaths)))
            pio = subprocess.Popen(["iostat", "-x", "1"],
                                   stdout = open(ioprofpath, "w"))
            pcpu = subprocess.Popen(["mpstat", "-P", "ALL", "1"],
                                    stdout = open(cpuprofpath, "w"))
        try:
            for fpath, nth in zip(fpaths, nthreadlist):
                ppool.append(subprocess.Popen(shlex.split(
                            self.cmd.format(fpath = fpath, iosize = iosize,
                                            iterate = iterate, nthread = nth)),
                                              stdout = subprocess.PIPE))
            for p in ppool:
                if p.wait() != 0:
                    sys.stderr.write("storage_measure failed\n")
                    sys.exit(1)
        finally:
            if self.statflg:
                pio.kill()
                pcpu.kill()
        reslist = []
        for p in ppool:
            resdict = {}
            for line in p.stdout:
                line = line.rstrip()
                for pat in self.patterns:
                    match = pat.match(line)
                    if match:
                        resdict[match.group(1)] = float(match.group(2))
                        break
            reslist.append(resdict)
        stime, ftime = lstime, fftime = reslist[0]["stime"], reslist[0]["ftime"]
        latency = reslist[0]["latency"]
        for res in reslist[1:]:
            latency += res["latency"]
            if stime > res["stime"]:
                stime = res["stime"]
            if lstime < res["stime"]:
                lstime = res["stime"]
            if ftime < res["ftime"]:
                ftime = res["ftime"]
            if fftime > res["ftime"]:
                fftime = res["ftime"]
        elatime = ftime - stime
        nlu = len(fpaths)
        iops =  (iterate * nthread) / elatime
        mbps = iops * iosize / 1000000
        latency /= nlu
        fullutil = (fftime - lstime) / elatime
        sys.stdout.write("""
elapsed = {0}(s)
mbps = {1}(MB/s)
iops = {2}(io/s)
latency = {3}(us)
fullutil = {4}

""".format(elatime, mbps, iops, latency, fullutil))
        return (elatime, mbps, iops, latency, fullutil)

outdir = "/data/local/keisuke/{0}".format(
    time.strftime("%Y%m%d%H%M%S", time.gmtime()))
os.mkdir(outdir)

lus = tuple(["/dev/fio{0}".format(c) for c in 'abcdefgh'])
measureinfos = ((9, 16, 10), (13, 13, 10), (16, 13, 7))

if __name__ == "__main__":
    dbpath = "/data/local/keisuke/stripedreadspec.db"
    srbench = stripedreadbenchmarker(outdir)

    # sequential read
    sys.stdout.write("sequential read\n")
    srbench.cmd = "./sequentialread {{fpath}} {{iosize}} {{iterate}} {{nthread}}"
    seqrecorder = iobenchrecorder(dbpath, "sequential_read",
                                  srbench.varnames, srbench.resnames,
                                  srbench.run)
    for iosize, iterate, maxnthread in measureinfos:
        iosize = 2 ** iosize
        iterate = 2 ** iterate
        for nlu in range(1, 9):
            nthreadlist = [2 ** i for i in range(maxnthread + 1)]
            while nthreadlist[0] < nlu:
                nthreadlist.pop(0)
            seqrecorder.allmeasure([(lus[:nlu], iosize, iterate, nthread)
                                    for nthread in nthreadlist])

    #random read
    sys.stdout.write("random read\n")
    srbench.cmd = "./randomread {{fpath}} {{iosize}} {{iterate}} {{nthread}}"
    randrecorder = iobenchrecorder(dbpath, "random_read",
                                   srbench.varnames, srbench.resnames,
                                   srbench.run)
    for iosize, iterate, maxnthread in measureinfos:
        iosize = 2 ** iosize
        iterate = 2 ** iterate
        for nlu in range(1, 9):
            nthreadlist = [2 ** i for i in range(maxnthread + 1)]
            while nthreadlist[0] < nlu:
                nthreadlist.pop(0)
            randrecorder.allmeasure([(lus[:nlu], iosize, iterate, nthread)
                                    for nthread in nthreadlist])
