#! /usr/bin/env python

import sys, os, shlex, re, sqlite3, itertools, time, glob
import subprocess as sp
from getreadspec import iobenchrecorder
from clearcache import *

class readbenchmarker(object):
    perfcmd = "perf stat -a -o {perfout} -- "

    def __init__(self, cmdtmp):
        self.cmdtmp = cmdtmp
        self.perfoutdir = None
        self.statoutdir = None

    def runmulti(self, valdict):
        assert self.cmdtmp, "Command template must be set."
        nthread = valdict["nthread"]
        bname = '_'.join([str(k) + str(v) for k, v in valdict.items()]) if valdict else "record"
        cmd = self.cmdtmp.format(**valdict)
        if self.perfoutdir:
            perfout = os.path.join(self.perfoutdir, bname + ".perf")
            cmd = self.perfcmd.format(perfout = perfout) + cmd
        sys.stderr.write("start : {0} {1}\n".format(cmd.split(" ", 1)[0],
                                                    ' '.join(["{0} = {1}".format(k, v)
                                                              for k, v in valdict.items()])))
        if self.statoutdir:
            iostatout = os.path.join(self.statoutdir, bname + ".io")
            mpstatout = os.path.join(self.statoutdir, bname + ".cpu")
            import profileutils
            with profileutils.cpuio_stat_watcher(iostatout, mpstatout):
                procs = [sp.Popen(shlex.split(cmd + str(i)),
                                  stdout = sp.PIPE, stderr = open("/dev/null", "w"))
                         for i in range(nthread)]
                for p in procs:
                    if p.wait() != 0:
                        sys.stderr.write("measure failed : {0}\n".format(p.returncode))
                        sys.exit(1)
        else:
            procs = [sp.Popen(shlex.split(cmd + str(i)),
                              stdout = sp.PIPE, stderr = open("/dev/null", "w"))
                     for i in range(nthread)]
            for p in procs:
                if p.wait() != 0:
                    sys.stderr.write("measure failed : {0}\n".format(p.returncode))
                    sys.exit(1)
        return self.proc_result([p.stdout for p in procs])

    def proc_result(self, outputs)
        res = {}
        respattern = re.compile(r"([a-z_]+)\s(\d+(?:\.\d*)?)")
        reskeys = ["start_time", "finish_time", "total_ops"]
        for k in reskeys: res[k] = []
        for output in outputs:
            for line in output:
                line = line.rstrip()
                match = respattern.match(line)
                if match and match.group(1) in res:
                    res[match.group(1)].append(float(match.group(2)))

        for key in reskeys:
            assert len(res[key]) == nthread, "could not collect result : " + key
        res["exec_time_sec"] = max(res["finish_time"]) - min(res["start_time"])
        res["bench_time_sec"] = min(res["finish_time"]) - max(res["start_time"])
        res["total_ops"] = sum(res["total_ops"])
        res["io_per_sec"] = res["total_ops"] / res["exec_time_sec"]
        res["mb_per_sec"] = res["io_per_sec"] * valdict["iosize"] / 2 ** 20
        res["usec_per_io"] = res["exec_time_sec"] * (10 ** 6) / res["total_ops"]
        del res["start_time"], res["finish_time"]
        for key in ["exec_time_sec", "bench_time_sec", "total_ops",
                    "io_per_sec", "mb_per_sec", "usec_per_io"]:
            sys.stderr.write("  {0} : {1}\n".format(key, res[key]))
        return res

def doreadbench(fpath, outdir, benchexe, valdicts, statflg = False):
    cmdtmp = benchexe + " -s {iosize} -m 1 -i {iterate} -t {timeout} " + fpath
    rbench = readbenchmarker(cmdtmp)
    bname = os.path.splitext(os.path.basename(fpath))[0]
    recorder = iobenchrecorder(os.path.join(outdir, "readspec_{0}.db".format(bname)))
    tblname = os.path.basename(benchexe)
    if tblname in recorder.tbldict: columns = recorder.tbldict[tblname]
    else:
        columns = (("iosize", "integer"),
                   ("nthread", "integer"),
                   ("exec_time_sec", "real"),
                   ("bench_time_sec", "real"),
                   ("total_ops", "integer"),
                   ("mb_per_sec", "real"),
                   ("io_per_sec", "real"),
                   ("usec_per_io", "real"))
        recorder.createtable(tblname, columns)
        columns = [v[0] for v in columns]
    for valdict in valdicts:
        clear_cache(2 ** 35)
        if statflg:
            bname = '_'.join([str(k) + str(v) for k, v in valdict.items()]) if valdict else "record"
            direc = os.path.join(outdir, tblname + bname)
            nums = [int(os.path.basename(d)) for d in glob.glob(os.path.join(direc, "[0-9]*"))]
            n = max(nums) + 1 if nums else 0
            rbench.statoutdir = os.path.join(direc, str(n))
            os.makedirs(rbench.statoutdir)
        else: rbench.statoutdir = None
        res = rbench.runmulti(valdict)
        res.update(valdict)
        del res["timeout"], res["iterate"]
        recorder.insert(tblname, res)

def main(datadir):
    iomax = 2 ** 38
    timeout = 30
    iosizes = [2 ** i for i in range(9, 22)]
    nthreads = [2 ** i for i in range(7)]
    maxnthread = max(nthreads)
    maxfsize = iomax / maxnthread
    valdicts = []
    for vals in itertools.product(iosizes, nthreads):
        valdicts.append({"iosize" : vals[0],
                         "nthread" : vals[1],
                         "timeout": timeout,
                         "iterate": maxfsize / vals[0]})

    prgdir = os.path.abspath(os.path.dirname(__file__) + "/../")
    outdir = "/data/local/keisuke/{0}".format(time.strftime("%Y%m%d%H%M%S", time.gmtime()))
    os.mkdir(outdir)
    fpath = os.path.join(datadir, "benchdata")
    # for i in range(max(nthreads)):
    #     procs = [sp.Popen(["util/genbenchdata",
    #                        fpath + str(i),
    #                        str(iomax / max(nthreads))])]
    #     for p in procs:
    #         if p.wait() != 0:
    #             sys.stderr.write("measure failed : {0}\n".format(p.returncode))
    #             sys.exit(1)

    for i in range(5):
        # sequential read
        sys.stdout.write("sequential read\n")
        doreadbench(fpath, outdir, os.path.join(prgdir, "sequentialread"), valdicts, True)
        time.sleep(300)

        # random read
        sys.stdout.write("random read\n")
        doreadbench(fpath, outdir, os.path.join(prgdir, "randomread"), valdicts, True)
        time.sleep(300)

    #sp.call(["/bin/rm", fpath + "*"])

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stdout.write("Usage : {0} fpath\n".format(sys.argv[0]))
        sys.exit(0)
    datadir = sys.argv[1]
    assert os.path.isdir(datadir), "datadir does not exist"

    main(datadir)
