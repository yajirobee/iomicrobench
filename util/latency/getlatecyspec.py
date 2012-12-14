#! /usr/bin/env python

import sys, os, subprocess, shlex, re, sqlite3
from collections import namedtuple

iototal = 2 ** 31
iosize = 512
cpucorelist = range(32)

elatimepat = re.compile(r"elapsed time = (\d+(?:\.\d*)?|\.\d+)\(us\)")
mbpspat = re.compile(r"mbps = (\d+(?:\.\d*)?|\.\d+)\(MB/s\)")
iopspat = re.compile(r"iops = (\d+(?:\.\d*)?|\.\d+)\(io/s\)")
latencypat = re.compile(r"latency = (\d+(?:\.\d*)?|\.\d+)\(us\)")

iospec = namedtuple("iospec", ["iosize_integer", "iterate_integer","core_integer",
                               "elapsed_real", "throughput_real", "iops_real", "latency_real"])

cpustat = namedtuple("cpustat", ["usedcore_text", "corename_text",
                                 "user_integer", "nice_integer",
                                 "sys_integer", "idle_integer",
                                 "iowait_integer", "irq_integer",
                                 "softirq_integer", "steal_integer", "guest_integer"])

def get_cpustat():
    cpustatdict = {}
    for line in open('/proc/stat'):
        k, v = line.strip().split(None, 1)
        if re.match(r'cpu\d*', k):
            if k == 'cpu':
                key = "all"
            else:
                key = k.replace("cpu", "")
            cpustatdict[key] = tuple([int(val) for val in v.split()])
    return cpustatdict

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stdout.write("Usage : {0} fpath\n".format(sys.argv[0]))
        sys.exit(1)
    fpath = sys.argv[1]
    dbpath = "/data/local/keisuke/latencyspec_{0}.db".format(os.path.basename(fpath))
    conn = sqlite3.connect(dbpath)
    schema = ','.join([name.replace('_', ' ') for name in iospec._fields])
    conn.execute("create table sequential_read({0})".format(schema))
    schema = ','.join([name.replace('_', ' ') for name in cpustat._fields])
    conn.execute("create table cpustat({0})".format(schema))
    for core in cpucorelist:
        iterate = iototal / iosize
        res = [0., 0., 0., 0.]
        cmd = "{0} {1} {2} {3} {4}".format("./sequentialreadaf",
                                           fpath, iosize, iterate, core)
        sys.stdout.write("start : {0}\n".format(cmd))
        sstat = get_cpustat()
        p = subprocess.Popen(shlex.split(cmd), stdout = subprocess.PIPE)
        if p.wait() != 0:
            sys.stderr.write("storage_measure failed\n")
            sys.exit(1)
        fstat = get_cpustat()
        for line in p.stdout:
            line = line.rstrip()
            for i, pat in enumerate([elatimepat, throughputpat, iopspat, latencypat]):
                match = pat.match(line)
                if match:
                    res[i] = float(match.group(1))
                    sys.stdout.write("{0}\n".format(line))
                    break
        res = iospec(iosize, iterate, core, *res)
        conn.execute("insert into sequential_read values (?,?,?,?,?,?,?)", res)
        conn.commit()
        res = cpustat(core, 'all',
                      *[fval - sval for sval, fval in zip(sstat.pop('all'), fstat.pop('all'))])
        conn.execute("insert into cpustat values (?,?,?,?,?,?,?,?,?,?,?)", res)
        conn.commit()
        for corename in sorted(sstat, key = lambda x: int(x)):
            res = cpustat(core, corename,
                          *[fval - sval for sval, fval in zip(sstat[corename], fstat[corename])])
            conn.execute("insert into cpustat values (?,?,?,?,?,?,?,?,?,?,?)", res)
            conn.commit()
    conn.close()
