#! /usr/bin/env python

import sys, os, subprocess, shlex, re, sqlite3, itertools, time

class readbenchmarker(object):
    respatterns = {
        "elapsed" : re.compile(r"elapsed = (\d+(?:\.\d*)?)(?:\(us\))?"),
        "mbps" : re.compile(r"mbps = (\d+(?:\.\d*)?)\(MB/s\)"),
        "iops" : re.compile(r"iops = (\d+(?:\.\d*)?)\(io/s\)"),
        "latency" : re.compile(r"latency = (\d+(?:\.\d*)?)(?:\(us\))?")
        }
    perfcmd = "perf stat -a -o {perfout} -- "

    def __init__(self, perfoutdir = None, statoutdir = None):
        self.perfoutdir = perfoutdir
        self.statoutdir = statoutdir
        self.cmdtmp = None

    def run(self, valdict):
        if not self.cmdtmp:
            sys.stderr.write("Command template must be set.\n")
            return None
        bname = '_'.join([str(k) + str(v) for k, v in valdict.items()]) if valdict else "record"
        cmd = self.cmdtmp.format(**valdict)
        if self.perfoutdir:
            perfout = "{0}/{1}.perf".format(self.perfoutdir, bname)
            cmd = self.perfcmd.format(perfout = perfout) + cmd
        sys.stderr.write("start : {0}\n".format(cmd))
        if self.statoutdir:
            iostatout = "{0}/{1}.io".format(self.statoutdir, bname)
            mpstatout = "{0}/{1}.cpu".format(self.statoutdir, bname)
            pio = subprocess.Popen(["iostat", "-x", "1"],
                                   stdout = open(iostatout, "w"))
            pmp = subprocess.Popen(["mpstat", "-P", "ALL", "1"],
                                   stdout = open(mpstatout, "w"))
        try:
            p = subprocess.Popen(shlex.split(cmd), stdout = subprocess.PIPE)
            if p.wait() != 0:
                sys.stderr.write("measure failed : {0}\n".format(p.returncode))
                sys.exit(1)
        finally:
            if self.statoutdir:
                pio.kill()
                pmp.kill()
        res = {}
        for k in self.respatterns:
            res[k] = None
        for line in p.stdout:
            sys.stderr.write("  {0}".format(line))
            line = line.rstrip()
            for k, pat in self.respatterns.items():
                match = pat.match(line)
                if match:
                    res[k] = float(match.group(1))
                    break
        return res

class iobenchrecorder(object):
    typedict = {"iosize" : "integer",
                "iterate" : "integer",
                "nthread" : "integer",
                "nlu" : "integer",
                "elapsed" : "real",
                "mbps" : "real",
                "iops" : "real",
                "latency" : "real",
                "fullutil" : "real",
                "fsize" : "integer"}

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
        schema = ','.join(["{0} {1}".format(k, self.typedict.get(k, ""))
                           for k in columns])
        self.conn.execute("create table {0}({1})".format(tblname, schema))
        self.tbldict[tblname] = columns

    def insert(self, tblname, valdict):
        columns = self.tbldict[tblname]
        orderedvals = [0 for i in columns]
        for k, v in valdict.items():
            orderedvals[columns.index(k)] = v
        self.conn.execute(("insert into {0} values ({1})"
                           .format(tblname, ','.join('?' * len(columns)))),
                          orderedvals)
        self.conn.commit()

def clear_dev_buffer():
    # clear storage side buffer
    ppool = []
    readcmd = "/data/local/keisuke/local/bin/sequentialread /dev/fio{0}2 {1} {2} {3}"
    for i in "abcdefgh":
        ppool.append(subprocess.Popen(shlex.split(readcmd.format(i, 2 ** 25, 4, 8)),
                                      stdout = open("/dev/null", "w")))
    for p in ppool:
        if p.wait() != 0:
            sys.stderr.write("iodrive read error : {0}\n".format(p.pid))
            sys.exit(1)

def clear_cache():
    ret = subprocess.call(["clearcache"])
    if ret != 0:
        sys.stderr.write("cache clear error\n")
        sys.exit(1)

def sequentialreadbench(fpath, outdir, valdicts):
    rbench = readbenchmarker(statoutdir = outdir)
    rbench.cmdtmp = ("./sequentialread "
                     "-s {{iosize}} -i {{iterate}} -m {{nthread}} {0}".format(fpath))
    bname = os.path.splitext(os.path.basename(fpath))[0]
    recorder = iobenchrecorder("{0}/readspec_{1}.db".format(outdir, bname))
    tblname = "sequential_read"
    if tblname in recorder.tbldict:
        columns = recorder.tbldict[tblname]
    else:
        columns = ("iosize", "iterate", "nthread", "elapsed", "mbps", "iops", "latency")
        recorder.createtable(tblname, columns)
    for valdict in valdicts:
        clear_dev_buffer()
        clear_cache()
        res = rbench.run(valdict)
        res.update(valdict)
        recorder.insert(tblname, res)

def randomreadbench(fpath, outdir, valdicts):
    rbench = readbenchmarker(statoutdir = outdir)
    rbench.cmdtmp = ("./randomread "
                     "-s {{iosize}} -i {{iterate}} -m {{nthread}} {0}".format(fpath))
    bname = os.path.splitext(os.path.basename(fpath))[0]
    recorder = iobenchrecorder("{0}/readspec_{1}.db".format(outdir, bname))
    tblname = "random_read"
    if tblname in recorder.tbldict:
        columns = recorder.tbldict[tblname]
    else:
        columns = ("iosize", "iterate", "nthread", "elapsed", "mbps", "iops", "latency")
        recorder.createtable(tblname, columns)
    for valdict in valdicts:
        clear_dev_buffer()
        clear_cache()
        res = rbench.run(valdict)
        res.update(valdict)
        recorder.insert(tblname, res)

'''
iomax = 2 ** 36
#parameters = ([2 ** i for i in range(9, 22)], [10000], [1])
nthreads = [2 ** i for i in range(11)]
parameters = (([2 ** 9], [2 ** 15], nthreads),
              ([2 ** 13], [2 ** 12], nthreads),
              ([2 ** 16], [2 ** 10], nthreads))
valdicts = []
for iosizes, iterates, nthreads in parameters:
    assert max(iosizes) * max(iterates) * max(nthreads) <= iomax, "excessive io size"
    for vals in itertools.product(iosizes, iterates, nthreads):
        valdicts.append({"iosize" : vals[0], "iterate" : vals[1], "nthread" : vals[2]})
'''
nthreads = [2 ** i for i in range(11)]
parameters = ({"iosize" : 2 ** 9, "totalsize" : 2 ** 33, "nthreads" : nthreads},
              {"iosize" : 2 ** 13, "totalsize" : 2 ** 34, "nthreads" : nthreads},
              {"iosize" : 2 ** 16, "totalsize" : 2 ** 36, "nthreads" : nthreads})
valdicts = []
for conf in parameters:
    for nth in conf["nthreads"]:
        valdicts.append({"iosize" : conf["iosize"],
                         "iterate" : conf["totalsize"] / (nth * conf["iosize"]),
                         "nthread" : nth})

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stdout.write("Usage : {0} fpath\n".format(sys.argv[0]))
        sys.exit(0)
    fpath = sys.argv[1]
    outdir = "/data/local/keisuke/{0}".format(time.strftime("%Y%m%d%H%M%S", time.gmtime()))
    os.mkdir(outdir)

    for i in range(5):
        # sequential read
        sys.stdout.write("sequential read\n")
        sequentialreadbench(fpath, outdir, valdicts)
        time.sleep(300)

        # random read
        sys.stdout.write("random read\n")
        randomreadbench(fpath, outdir, valdicts)
        time.sleep(300)
