#! /usr/bin/env python

import sys, os, shlex, re, sqlite3, itertools, time, glob
import subprocess as sp
from clearcache import *

class iobenchrecorder(object):
    def __init__(self, dbpath):
        self.conn = sqlite3.connect(dbpath)
        self.tbldict = {}
        for r in self.conn.execute("select name, sql from sqlite_master where type = 'table'"):
            match = re.search(".*\((.*)\).*", r[1])
            columns = match.group(1).split(',')
            self.tbldict[r[0]] = [v.strip().split()[0] for v in columns]

    def createtable(self, tblname, columns):
        if tblname in self.tbldict:
            sys.stderr.write("table already exist : {0}\n".format(tblname))
            return
        schema = ','.join([' '.join(v) for v in columns])
        self.conn.execute("create table {0}({1})".format(tblname, schema))
        self.tbldict[tblname] = [v[0] for v in columns]

    def insert(self, tblname, valdict):
        columns = self.tbldict[tblname]
        orderedvals = [0 for i in columns]
        for k, v in valdict.items(): orderedvals[columns.index(k)] = v
        valmask = ','.join('?' * len(columns))
        self.conn.execute("insert into {0} values ({1})".format(tblname, valmask),
                          orderedvals)
        self.conn.commit()

class readbenchmarker(object):
    perfcmd = "perf stat -a -o {perfout} -- "

    def __init__(self, perfoutdir = None, statoutdir = None):
        self.perfoutdir = perfoutdir
        self.statoutdir = statoutdir
        self.cmdtmp = None

    def runmulti(self, valdict):
        if not self.cmdtmp:
            sys.stderr.write("Command template must be set.\n")
            return None
        nthread = valdict["nthread"]
        bname = '_'.join([str(k) + str(v) for k, v in valdict.items()]) if valdict else "record"
        cmd = self.cmdtmp.format(**valdict)
        if self.perfoutdir:
            perfout = "{0}/{1}.perf".format(self.perfoutdir, bname)
            cmd = self.perfcmd.format(perfout = perfout) + cmd
        sys.stderr.write("start : {0} {1}\n".format(cmd.split(" ", 1)[0],
                                                    ' '.join(["{0} = {1}".format(k, v)
                                                              for k, v in valdict.items()])))
        if self.statoutdir:
            iostatout = "{0}/{1}.io".format(self.statoutdir, bname)
            mpstatout = "{0}/{1}.cpu".format(self.statoutdir, bname)
            pio = sp.Popen(["iostat", "-x", "1"], stdout = open(iostatout, "w"))
            pmp = sp.Popen(["mpstat", "-P", "ALL", "1"], stdout = open(mpstatout, "w"))
        try:
            procs = [sp.Popen(shlex.split(cmd + str(i)),
                              stdout = sp.PIPE, stderr = open("/dev/null", "w"))
                     for i in range(nthread)]
            for p in procs:
                if p.wait() != 0:
                    sys.stderr.write("measure failed : {0}\n".format(p.returncode))
                    sys.exit(1)
        finally:
            if self.statoutdir:
                pio.kill()
                pmp.kill()
        res = {}
        respattern = re.compile(r"([a-z_]+)\s(\d+(?:\.\d*)?)")
        reskeys = ["start_time", "finish_time", "total_ops"]
        for k in reskeys: res[k] = []
        for p in procs:
            for line in p.stdout:
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

def doreadbench(fpath, outdir, cmdtmp, valdicts, statflg = False):
    rbench = readbenchmarker()
    rbench.cmdtmp = cmdtmp
    bname = os.path.splitext(os.path.basename(fpath))[0]
    recorder = iobenchrecorder("{0}/readspec_{1}.db".format(outdir, bname))
    tblname = cmdtmp.split(None, 1)[0].rsplit("/", 1)[1]
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
            direc = "{0}/{1}{2}".format(outdir, tblname, bname)
            nums = [int(os.path.basename(d)) for d in glob.glob("{0}/[0-9]*".format(direc))]
            n = max(nums) + 1 if nums else 0
            os.makedirs(rbench.statoutdir)
            rbench.statoutdir = "{0}/{1}".format(direc, n)
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
    prgdir = os.path.abspath(os.path.dirname(__file__) + "/../") + "/"
    outdir = "/data/local/keisuke/{0}".format(time.strftime("%Y%m%d%H%M%S", time.gmtime()))
    os.mkdir(outdir)
    fpath = "{0}/benchdata".format(datadir)
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
        cmdtmp = (prgdir + "sequentialread "
                  "-s {{iosize}} -m 1 "
                  "-i {{iterate}} -t {{timeout}} {0}".format(fpath))
        doreadbench(fpath, outdir, cmdtmp, valdicts)
        time.sleep(300)

        # random read
        sys.stdout.write("random read\n")
        cmdtmp = (prgdir + "randomread "
                  "-s {{iosize}} -m 1 "
                  "-i {{iterate}} -t {{timeout}} {0}".format(fpath))
        doreadbench(fpath, outdir, cmdtmp, valdicts)
        time.sleep(300)

    sp.call(["/bin/rm", fpath + "/*"])

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stdout.write("Usage : {0} fpath\n".format(sys.argv[0]))
        sys.exit(0)
    datadir = sys.argv[1]
    assert os.path.isdir(datadir), "datadir does not exist"

    main(datadir)
