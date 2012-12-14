#! /usr/bin/env python

import sys, os, subprocess, shlex, re, sqlite3
from collections import namedtuple

fpath = "/dev/fioa"
iototal = 2 ** 31
iosizelist = [2 ** (i + 9) for i in range(13)]

elatimepat = re.compile(r"elapsed = (\d+(?:\.\d*)?|\.\d+)\(us\)")
mbpspat = re.compile(r"mbps = (\d+(?:\.\d*)?|\.\d+)\(MB/s\)")
iopspat = re.compile(r"iops = (\d+(?:\.\d*)?|\.\d+)\(io/s\)")
patterns = (elatimepat, mbpspat, iopspat)
iospec = ["iosize integer", "iterate integer", "elapsed real", "mbps real", "iops real"])
modes = ["sequential_read", "random_read", "sequential_write", "random_write"]

if __name__ == "__main__":
    dbpath = "/data/local/keisuke/storagespec_{0}.db".format(os.path.basename(fpath))
    conn = sqlite3.connect(dbpath)
    schema = ','.join(iospec)
    for mode in modes:
        conn.execute("create table {0}({1})".format(data[mode, schema)))
        for iosize in iosizelist:
            iterate = iototal / iosize
            res = [0., 0., 0.]
            cmd = "./storage_measure {0} {1} {2} {3}".format(mode, fpath, iosize, iterate)
            sys.stdout.write("start : {0}\n".format(cmd))
            p = subprocess.Popen(shlex.split(cmd), stdout = subprocess.PIPE)
            if p.wait() != 0:
                sys.stderr.write("storage_measure failed\n")
                sys.exit(1)
            for line in p.stdout:
                line = line.rstrip()
                for i, pat in enumerate(patterns):
                    match = pat.match(line)
                    if match:
                        res[i] = float(match.group(1))
                        sys.stdout.write("{0}\n".format(line))
            conn.execute(("insert into {0} values ({1})"
                          .format(mode, ','.join('?' * len(iospec)))),
                         [iosize, iterate] + res)
            conn.commit()
    conn.close()
