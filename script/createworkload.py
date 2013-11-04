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
        tasks.append((filepath, mode, offset, iosize, iteration))
        offset += iosize * iteration
    return tasks

def create_workload(output, nthread):
    readfiles = [os.path.join(datadir, "benchdata" + str(i)) for i in range(32)]
    writefiles = [os.path.join(datadir, "benchdata" + str(i)) for i in range(32, 64)]
    numtasks = 2000
    readtasksdict = dict([(i, []) for i in range(len(nthread))])
    writetasksdict = dict([(i, []) for i in range(len(nthread))])
    with open(output, "w") as fo:
        for i in range(len(numtasks)):
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
        sys.stderr.write("Usage : {0} output\n".format(sys.argv[0]))
        sys.exit(1)

    output = sys.argv[1]
    create_workload(output, 16)
