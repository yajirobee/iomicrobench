#! /usr/bin/env python

import sys, os, re, subprocess, shlex, time, sqlite3

class benchmarker(object):
    patterns = []

    def __init__(self, fpath, outdir = ".",
                 perfflg = False, statflg = False):
        self.fpath = fpath
        self.outdir = outdir
        self.perfflg = perfflg
        self.statflg = statflg

    def setcmdtemplate(self, cmdtmp):
        self.cmdtmp = cmdtmp

    def setcmd(self, prg):
        self.cmd = self.cmdtmp.format(prg, self.fpath)

    def run(self, vals):
        bname = '_'.join([os.path.basename(self.fpath)] +
                         [str(k) + str(v) for k, v in zip(varnames, vals)])
        if self.perfflg:
            perfpath = bname + ".perf"
            self.cmd = "perf record -a -o {0} ".format(perfpath) + self.cmd
        cmd = self.cmd.format(*vals)
        sys.stdout.write("start : {0}\n".format(cmd))
        if self.statflg:
            ioprofpath = bname + ".io"
            cpuprofpath = bname + ".cpu"
            pio = subprocess.Popen(["iostat", "-x", "1"],
                                   stdout = open(ioprofpath, "w"))
            pcpu = subprocess.Popen(["mpstat", "-P", "ALL", "1"],
                                    stdout = open(cpuprofpath, "w"))
        try:
            p = subprocess.Popen(shlex.split(cmd), stdout = subprocess.PIPE)
            if p.wait() != 0:
                sys.stderr.write("measure failed\n")
                sys.exit(1)
        finally:
            if self.statflg:
                pio.kill()
                pcpu.kill()
        res = [0.0 for i in resnames]
        for line in p.stdout:
            line = line.rstrip()
            for i, pat in enumerate(self.patterns):
                match = pat.match(line)
                if match:
                    res[i] = float(match.group(1))
                    sys.stdout.write("{0}\n".format(line))
                    break
        return res

class readbenchmarker(benchmarker):
    elapat = re.compile(r"elapsed = (\d+(?:\.\d*)?)(?:\(us\))?")
    mbpspat = re.compile(r"mbps = (\d+(?:\.\d*)?)\(MB/s\)")
    iopspat = re.compile(r"iops = (\d+(?:\.\d*)?)\(io/s\)")
    latencypat = re.compile(r"latency = (\d+(?:\.\d*)?)(?:\(us\))?")
    patterns = (elapat, mbpspat, iopspat, latencypat)

    varnames = ["iosize", "iterate", "nthread"]
    resnames = ["elapsed", "mbps", "iops", "latency"]

    def __init__(self, fpath, outdir = ".",
                 perfflg = False, statflg = False):
        super(readbenchmarker, self).__init__(fpath, outdir, perfflg, statflg)
        self.setcmdtemplate("{0} {1} {{0}} {{1}} {{2}}")

class iobenchrecorder(object):
    typedict = {"iosize" : "integer", "iterate" : "integer",
                "nthread" : "integer", "nlu" : "integer",
                "elapsed" : "real", "mbps" : "real",
                "iops" : "real", "latency" : "real", "fullutil" : "real",
                "fsize" : "integer"}

    def __init__(self, dbpath, tblname, varnames, resnames, mfunc):
        self.tblname = tblname
        self.mfunc = mfunc
        self.ncol = len(varnames + resnames)
        self.conn = sqlite3.connect(dbpath)
        schema = ','.join(["{0} {1}".format(key, self.typedict.get(key, ""))
                           for key in varnames + resnames])
        self.conn.execute("create table {0}({1})".format(tblname, schema))

    def allmeasure(self, confs):
        for conf in confs:
            res = self.mfunc(*conf)
            res = list(conf) + list(res)
            self.conn.execute(("insert into {0} values ({1})"
                               .format(self.tblname,
                                       ','.join('?' * self.ncol))), res)
            self.conn.commit()
