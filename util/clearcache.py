#! /usr/bin/env python

import sys, os, subprocess, shlex
import multiprocessing as mp

devices = ["/dev/fio{0}2".format(i) for i in "abcdefgh"]
devicevolume = 2 ** 35

def clear_iodrive_buffer(size, iosize = 2 ** 25, nthread = 4):
    # clear storage side buffer
    assert iosize % (2 ** 10) == 0, "iosize should be multiple of 1024"
    assert nthread & (nthread - 1) == 0, "nthread should be power of 2"
    ppool = []
    readcmd = ("/data/local/keisuke/local/bin/sequentialread -d "
               "-s {iosize} -i {iterate} -m {nthread} {filepath}")
    size = min(size, devicevolume)
    iterate = max(size / (iosize * nthread), 1)
    for dev in devices:
        cmd = shlex.split(readcmd.format(iosize = iosize, iterate = iterate,
                                         nthread = nthread, filepath = dev))
        ppool.append(subprocess.Popen(cmd,
                                      stdout = open("/dev/null", "w"),
                                      stderr = subprocess.STDOUT))
    for p in ppool:
        if p.wait() != 0:
            sys.stderr.write("iodrive read error : {0}\n".format(p.pid))
            sys.exit(1)

def sequentialreadio(fd, iosize, iterateion):
    for i in range(iteration): os.read(fd, iosize)

def multiprocessing_helper(args):
    return args[0](*args[1:])

def clear_dev_buffer(size, iosize = 2 ** 25, nthread = 32):
    assert iosize % (2 ** 10) == 0, "iosize must be multiple of 1024"
    assert nthread & (nthread - 1) == 0, "nthread must be power of 2"
    size = min(size, devicevolume)
    threadperdev = max(nthread / len(devices), 1)
    sizeperthread = size / threadperdev
    assert sizeperthread >= iosize, "size must be larger than iosize"
    iterperthread = sizeperthread / iosize
    filedescriptors = []
    for dev in devices:
        fds = [os.open(dev, os.O_RDONLY | os.O_DIRECT) for i in range(threadperdev)]
        for i, fd in enumerate(fds): os.lseek(fd, sizeperthread * i, os.SEEK_SET)
        filedescriptors.extend(fds)
    argslist = [(sequentialreadio, fd, iosize, iterperthread) for fd in filedescriptors]
    pool = mp.Pool(nthread)
    pool.map(multiprocessing_helper, argslist)

def clear_os_cache():
    subprocess.call(["sync"])
    subprocess.call(["clearcache"])

def clear_cache(size = 2 ** 30):
    clear_iodrive__buffer(size)
    clear_os_cache()
