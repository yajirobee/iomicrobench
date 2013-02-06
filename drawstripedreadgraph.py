#! /usr/bin/env python

import sys, os, sqlite3
import plotutil as pu

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

    gp = pu.gpinit(terminaltype)
    #draw nlu-spec graph
    gp('set nologscale x')
    distinctsql = "select distinct nthread from {0} where iosize={1}"
    nthreadlistlist = [[r[0] for r in conn.execute(distinctsql.format(tbl, iosize))]
                       for tbl in tables]
    gp.xlabel("nlu")
    for col in cols:
        gp('set title "{0} iosize = {1}"'.format(col, iosize))
        gp.ylabel(col)
        if col == "mbps" or col == "iops":
            gp('set key left top')
        else:
            gp('set key right top')
        figpath = "{0}_xnlu.{1}".format(col, terminaltype)
        gp('set output "{0}"'.format(figpath))
        gds = []
        for tbl, nth in zip(tables, nthreadlistlist):
            query = ("select nlu,{0} from {1} where nthread={{nthread}} and iosize={2}"
                     .format(col, tbl, iosize))
            gds.extend(pu.query2data(conn, query, nthread = nth,
                                     title = "{0} {1} = {{{1}}}".format(tbl, "nlu"),
                                     with_ = "linespoints"))
        sys.stdout.write('draw : {0}\n'.format(figpath))
        gp.plot(*gds)

    #draw nthread-spec graph
    gp('set logscale x')
    distinctsql = "select distinct nlu from {0} where iosize={1}"
    nlulistlist = [[r[0] for r in conn.execute(distinctsql.format(tbl, iosize))]
                   for tbl in tables]
    gp.xlabel("nthread")
    for col in cols:
        gp('set title "{0} iosize = {1}"'.format(col, iosize))
        gp.ylabel(col)
        if col == "mbps" or col == "latency":
            gp('set key left top')
        else:
            gp('set key right top')
        figpath = "{0}_xnthread.{1}".format(col, terminaltype)
        gp('set output "{0}"'.format(figpath))
        gds = []
        for tbl, nlu in zip(tables, nlulistlist):
            query = ("select nthread,{0} from {1} where nlu={{nlu}} and iosize={2}"
                     .format(col, tbl, iosize))
            gds.extend(pu.query2data(conn, query, nlu = nlu,
                                     title = "{0} {1} = {{{1}}}".format(tbl, "nthread"),
                                     with_ = "linespoints"))
        sys.stdout.write('draw : {0}\n'.format(figpath))
        gp.plot(*gds)
    gp.close()
    conn.close()
