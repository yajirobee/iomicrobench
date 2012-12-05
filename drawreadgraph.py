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
    cols = ["elapsed", "mbps", "iops", "latency"]
    units = {"elapsed" : "(us)",
             "mbps" : "(MB/s)",
             "iops" : "",
             "latency" : "(us)"}
    tables = ["sequential_read", "random_read"]
    fpath = os.path.dirname(dbpath) + "/" + os.path.splitext(dbpath)[0].rsplit('_', 1)[1]

    gp = plotutil.gpinit(terminaltype)
    gp('set logscale x')
    for table in tables:
        gp('set title "{0}"'.format(table))
        #draw iosize-spec graph
        nthreadlist = [r[0] for r in conn.execute("select distinct nthread from {0}".format(table))]
        gp.xlabel("io size (B)")
        for col in cols:
            gp.ylabel("{0} {1}".format(col, units[col]))
            if col == "mbps" or col == "latency":
                gp('set key left top')
            else:
                gp('set key right top')
            figpath = "{0}_{1}_{2}_xiosize.{3}".format(fpath, table, col, terminaltype)
            gp('set output "{0}"'.format(figpath))
            query = "select iosize,{0} from {1} where nthread={{nthread}}".format(col, table)
            gd = plotutil.query2data(conn, query, nthread = nthreadlist,
                                     with_ = "linespoints")
            sys.stdout.write('draw : {0}\n'.format(figpath))
            gp.plot(*gd)

        #draw nthread-spec graph
        iosizelist = [r[0] for r in conn.execute("select distinct iosize from {0}".format(table))]
        gp.xlabel("nthread")
        for col in cols:
            gp.ylabel("{0} {1}".format(col, units[col]))
            if col == "mbps" or col == "latency" or col == "iops":
                gp('set key left top')
            else:
                gp('set key right top')
            figpath = "{0}_{1}_{2}_xnthread.{3}".format(fpath, table, col, terminaltype)
            gp('set output "{0}"'.format(figpath))
            query = "select nthread,{0} from {1} where iosize={{iosize}}".format(col, table)
            gd = plotutil.query2data(conn, query, iosize = iosizelist,
                                     with_ = "linespoints")
            sys.stdout.write('draw : {0}\n'.format(figpath))
            gp.plot(*gd)
    gp.close()
    conn.close()
