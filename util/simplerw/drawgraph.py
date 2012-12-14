#! /usr/bin/env python

import sys, os, sqlite3
import plotutil

if __name__ == "__main__":
    if len(sys.argv) == 2:
        dbpath = sys.argv[1]
        terminaltype = "png"
    elif len(sys.argv) == 3:
        dbpath = sys.argv[1]
        terminaltype = sys.argv[2]
    else:
        sys.stdout.write("Usage : {0} dbpath [eps|png]\n".format(sys.argv[0]))
        sys.exit(1)

    if terminaltype != "png" and terminaltype != "eps":
        sys.stdout.write("wrong terminal type\n")
        sys.exit(1)

    conn = sqlite3.connect(dbpath)
    cols = ["elapsedtime", "mbps", "iops"]
    units = {"elapsed" : "(us)",
             "mbps" : "(MB/s)",
             "iops" : ""}
    tables = ["sequential_read", "random_read", "sequential_write", "random_write"]

    for i, col in enumerate(cols):
        gp = plotutil.gpinit(terminaltype)
        gp.xlabel("io size (B)")
        gp.ylabel("{0} {1}".format(col, units[col]))
        fpath = os.path.dirname(dbpath) + "/" + os.path.splitext(dbpath)[0].rsplit('_', 1)[1]
        figpath = "{0}_{1}.{2}".format(fpath, col, terminaltype)
        gp('set title "{0}"'.format(col))
        gp('set output "{0}"'.format(figpath))
        gp('set logscale x')
        if col == "mbps":
            gp('set key left top')
        query = "select iosize, {0} from {{table}}".format(col)
        gd = plotutil.query2data(conn, query, table = tables, with_ = "linespoints")
        gp.plot(*gd)
        gp.close()
