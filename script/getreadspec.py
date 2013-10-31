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

    def __init__(self, cmdtmp):
        self.cmdtmp = cmdtmp
        self.perfoutdir = None
        self.statoutdir = None

    def run(self, valdict):
        assert self.cmdtmp, "Command template must be set."
        bname = '_'.join([str(k) + str(v) for k, v in valdict.items()]) if valdict else "record"
        cmd = self.cmdtmp.format(**valdict)
        if self.perfoutdir:
            perfout = "{0}/{1}.perf".format(self.perfoutdir, bname)
            cmd = self.perfcmd.format(perfout = perfout) + cmd
        sys.stderr.write("start : {0}\n".format(cmd))
        if self.statoutdir:
            iostatout = "{0}/{1}.io".format(self.statoutdir, bname)
            mpstatout = "{0}/{1}.cpu".format(self.statoutdir, bname)
            import profileutils
            with profileutils.cpuio_stat_watcher(iostatout, mpstatout):
                p = sp.Popen(shlex.split(cmd), stdout = sp.PIPE)
                ret = p.wait()
        else:
            p = sp.Popen(shlex.split(cmd), stdout = sp.PIPE)
            ret = p.wait()
        if ret != 0:
            sys.stderr.write("measure failed : {0}\n".format(p.returncode))
            sys.exit(1)
        return self.proc_result(p.stdout)

    def proc_result(self, output):
        res = {}
        respattern = re.compile(r"([a-z_]+)\s(\d+(?:\.\d*)?)")
        reskeys = ["exec_time_sec", "total_ops",
                   "mb_per_sec", "io_per_sec", "usec_per_io"]
        for k in reskeys: res[k] = None
        for line in output:
            sys.stderr.write("  {0}".format(line))
            line = line.rstrip()
            match = respattern.match(line)
            if match and match.group(1) in res:
                res[match.group(1)] = float(match.group(2))
        return res

def doreadbench(fpath, outdir, benchexe, valdicts, statflg = False):
    cmdtmp = benchexe + " -s {iosize} -m {nthread} -i {iterate} -t {timeout} " + fpath
    rbench = readbenchmarker(cmdtmp)
    bname = os.path.splitext(os.path.basename(fpath))[0]
    recorder = iobenchrecorder("{0}/readspec_{1}.db".format(outdir, bname))
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
        clear_cache(2 ** 30)
        if statflg:
            bname = '_'.join([str(k) + str(v) for k, v in valdict.items()]) if valdict else "record"
            direc = os.path.join(outdir, tblname + bname)
            nums = [int(os.path.basename(d)) for d in glob.glob("{0}/[0-9]*".format(direc))]
            n = max(nums) + 1 if nums else 0
            rbench.statoutdir = "{0}/{1}".format(direc, n)
            os.makedirs(rbench.statoutdir)
        else: rbench.statoutdir = None
        res = rbench.run(valdict)
        res.update(valdict)
        del res["timeout"], res["iterate"]
        recorder.insert(tblname, res)

def main(fpath):
    iomax = 2 ** 36
    timeout = 30
    iosizes = [2 ** i for i in range(9, 22)]
    nthreads = [2 ** i for i in range(11)]
    valdicts = []
    for vals in itertools.product(iosizes, nthreads):
        valdicts.append({"iosize" : vals[0],
                         "nthread" : vals[1],
                         "timeout": timeout,
                         "iterate": iomax / (vals[0] * vals[1])})

    prgdir = os.path.abspath(os.path.dirname(__file__) + "/../")
    outdir = "/data/local/keisuke/{0}".format(time.strftime("%Y%m%d%H%M%S", time.gmtime()))
    os.mkdir(outdir)

    for i in range(5):
        # sequential read
        sys.stdout.write("sequential read\n")
        doreadbench(fpath, outdir, os.path.join(prgdir, "sequentialread"), valdicts, True)
        time.sleep(300)

        # random read
        sys.stdout.write("random read\n")
        doreadbench(fpath, outdir, os.path.join(prgdir, "randomread"), valdicts, True)
        time.sleep(300)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stdout.write("Usage : {0} fpath\n".format(sys.argv[0]))
        sys.exit(0)
    fpath = sys.argv[1]

    main(fpath)
