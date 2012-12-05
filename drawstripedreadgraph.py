#! /usr/bin/env python

import sys, os, sqlite3
import plotutil

iosize = 2 ** 9

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
    cols = ["elapsed", "mbps", "iops", "latency"]
    tables = ["sequential_read", "random_read"]

    gp = plotutil.gpinit(terminaltype)
    for table in tables:
        gp('set title "{0} iosize = {1}"'.format(table, iosize))
        #draw nlu-spec graph
        gp('set nologscale x')
        nthreadlist = [r[0] for r in conn.execute("select distinct nthread from {0} where iosize={1}".format(table, iosize))]
        gp.xlabel("nlu")
        for col in cols:
            gp.ylabel(col)
            if col == "mbps" or col == "iops":
                gp('set key left top')
            else:
                gp('set key right top')
            figpath = "{0}_{1}_xnlu.{2}".format(table, col, terminaltype)
            gp('set output "{0}"'.format(figpath))
            query = "select nlu,{0} from {1} where nthread={{nthread}} and iosize={2}".format(col, table, iosize)
            gd = plotutil.query2data(conn, query, nthread = nthreadlist,
                                       with_ = "linespoints")
            sys.stdout.write('draw : {0}\n'.format(figpath))
            gp.plot(*gd)

        #draw nthread-spec graph
        gp('set logscale x')
        nlulist = [r[0] for r in conn.execute("select distinct nlu from {0} where iosize={1}".format(table, iosize))]
        gp.xlabel("nthread")
        for col in cols:
            gp.ylabel(col)
            if col == "mbps" or col == "latency":
                gp('set key left top')
            else:
                gp('set key right top')
            figpath = "{0}_{1}_xnthread.{2}".format(table, col, terminaltype)
            gp('set output "{0}"'.format(figpath))
            query = "select nthread,{0} from {1} where nlu={{nlu}} and iosize={2}".format(col, table, iosize)
            gd = plotutil.query2data(conn, query, nlu = nlulist,
                                       with_ = "linespoints")
            sys.stdout.write('draw : {0}\n'.format(figpath))
            gp.plot(*gd)
    gp.close()
    conn.close()
