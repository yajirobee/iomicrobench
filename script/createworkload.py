#! /usr/bin/env python

import sys, os

datadir = "/data/iod8raid0/tpchdata"

def create_tasks_fromfile(filepath, mode):
    iosize = 1 << 13
    maxiter = 1 << 10
    fsize = os.path.getsize(filepath)
    tasks = []
    offset = 0
    while offset + iosize <= fsize:
        iteration = min((fsize - offset) / iosize, maxiter)
        tasks.append((filepath, mode, str(offset), str(iosize), str(iteration)))
        offset += iosize * iteration
    return tasks

def create_workload(output, ntasks, nthread):
    readfiles = [os.path.join(datadir, "benchdata" + str(i)) for i in range(32)]
    writefiles = [os.path.join(datadir, "benchdata" + str(i)) for i in range(32, 64)]
    readtasksdict = dict([(i, []) for i in range(nthread)])
    writetasksdict = dict([(i, []) for i in range(nthread)])
    with open(output, "w") as fo:
        for i in range(ntasks):
            idx = i % nthread
            if (i / nthread) % 2 = 0:
                if not readtasksdict[idx]:
                    f = readfiles.pop(0)
                    readtasksdict[idx] = create_tasks_fromfile(f, "R")
                task = readtasksdict[i % nthread].pop(0)
            else:
                if not writetasksdict[idx]:
                    f = writefiles.pop(0)
                    writetasksdict[idx] = create_tasks_fromfile(f, "W")
                task = writetasksdict[i % nthread].pop(0)
            fo.write("\t".join(task) + "\n")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.stderr.write("Usage : {0} output [nthread]\n".format(sys.argv[0]))
        sys.exit(1)

    output = sys.argv[1]
    nthread = int(sys.argv) if len(sys.argv) >= 3 else 1
    create_workload(output, 4000, nthread)
