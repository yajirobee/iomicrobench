#! /usr/bin/env python

import sys, os, sqlite3
import plotutil as pu

if __name__ == "__main__":
    if len(sys.argv) == 2:
        dbpath = os.path.abspath(sys.argv[1])
        terminaltype = "png"
    elif len(sys.argv) == 3:
        dbpath = os.path.abspath(sys.argv[1])
        terminaltype = sys.argv[2]
    else:
        sys.stdout.write("Usage : {0} dbpath [eps|png]\n".format(sys.argv[0]))
        sys.exit(1)

    if terminaltype != "png" and terminaltype != "eps":
        sys.stdout.write("wrong terminal type\n")
        sys.exit(1)

    conn = sqlite3.connect(dbpath)
    cols = ["elapsed", "mbps", "iops", "latency"]
    units = {"elapsed" : "(us)",
             "mbps" : "(MB/s)",
             "iops" : "",
             "latency" : "(us)"}
    tables = ["random_read"]
    fpath = os.path.dirname(dbpath) + "/" + os.path.splitext(dbpath)[0].rsplit('_', 1)[1]

    gp = pu.gpinit(terminaltype)
    gp('set logscale x')
    #draw iosize-spec graph
    gp.xlabel("fsize (B)")
    for col in cols:
        gp('set title "{0}"'.format(col))
        gp.ylabel("{0} {1}".format(col, units[col]))
        if col == "mbps" or col == "latency":
            gp('set key left top')
        else:
            gp('set key right top')
        figpath = "{0}_{1}.{2}".format(fpath, col, terminaltype)
        gp('set output "{0}"'.format(figpath))
        gds = []
        for tbl in tables:
            query = "select fsize,{0} from {1}".format(col, tbl)
            gds.extend(pu.query2data(conn, query, with_ = "linespoints"))
        sys.stdout.write('draw : {0}\n'.format(figpath))
        gp.plot(*gds)

    gp.close()
    conn.close()
