#! /usr/bin/env python

import sys, os, sqlite3
from plotutil import gpinit, query2gds

slide = False
slide = 1

cols = [{"name": "mbps", "title": "Throughput (MB/s)", "ylabel": "MB/s"},
        {"name": "iops", "title": "Throughput (IOPS)", "ylabel": "IOPS"},
        {"name": "latency", "title": "Access latency", "ylabel": "latency [ns]"}]
units = {"elapsed" : "[us]",
         "mbps" : "[MB/s]",
         "iops" : "",
         "latency" : "[us]"}
tables = ["sequential_read", "random_read"]

def plot_iosize_spec(dbpath, terminaltype = "png"):
    "draw iosize-spec graph"
    conn = sqlite3.connect(dbpath)
    fpath = os.path.dirname(dbpath) + "/" + os.path.splitext(dbpath)[0].rsplit('_', 1)[1]
    gp = gpinit(terminaltype)
    gp('set logscale x')
    gp('set grid')
    if slide:
        if "eps" == terminaltype:
            gp('set termoption font "Times-Roman,28"')
            plotprefdict = {"with_" : "linespoints lt 1 lw 6" }
        elif "png" == terminaltype:
            gp('set termoption font "Times-Roman,16"')
            plotprefdict = {"with_" : "linespoints lw 2"}
    else:
        plotprefdict = {"with_" : "linespoints" }
    nthreadlistlist = [[r[0] for r in
                        conn.execute("select distinct nthread from {0}".format(tbl))]
                       for tbl in tables]
    gp.xlabel("I/O size [B]")
    gp('set format x "%.0s%cB"')
    for col in cols:
        gp('set title "{0}"'.format(col["title"]))
        gp.ylabel(col["ylabel"])
        if col["name"] == "mbps" or col["name"] == "latency":
            gp('set key left top')
        else:
            gp('set key right top')
        figpath = "{0}_{1}_xiosize.{2}".format(fpath, col["name"], terminaltype)
        gp('set output "{0}"'.format(figpath))
        gds = []
        for tbl, nth in zip(tables, nthreadlistlist):
            query = ("select iosize,avg({0}) from {1} "
                     "where nthread={{nthread}}"
                     "group by iosize,nthread".format(col["name"], tbl))
            gds.extend(query2gds(conn, query, nthread = nth,
                                 title = "{0} {1} = {{{1}}}".format(tbl, "nthread"),
                                 with_ = "linespoints"))
        sys.stdout.write('draw : {0}\n'.format(figpath))
        gp.plot(*gds)
    gp.close()
    conn.close()

def plot_nthread_spec(dbpath, terminaltype = "png"):
    "draw nthread-spec graph"
    conn = sqlite3.connect(dbpath)
    fpath = os.path.dirname(dbpath) + "/" + os.path.splitext(dbpath)[0].rsplit('_', 1)[1]
    gp = gpinit(terminaltype)
    gp('set logscale x')
    gp('set grid')
    if slide:
        if "eps" == terminaltype:
            gp('set termoption font "Times-Roman,28"')
            plotprefdict = {"with_" : "linespoints lt 1 lw 6" }
        elif "png" == terminaltype:
            gp('set termoption font "Times-Roman,18"')
            plotprefdict = {"with_" : "linespoints lw 2"}
    else:
        plotprefdict = {"with_" : "linespoints" }
    iosizelistlist = [[r[0] for r in
                       conn.execute("select distinct iosize from {0}".format(tbl))]
                      for tbl in tables]
    gp.xlabel("nthread")
    for col in cols:
        gp('set title "{0}"'.format(col["title"]))
        gp.ylabel(col["ylabel"])
        if col["name"] == "latency":
            gp('set key left top')
        else:
            gp('set key right top')
        figpath = "{0}_{1}_xnthread.{2}".format(fpath, col["name"], terminaltype)
        gp('set output "{0}"'.format(figpath))
        gds = []
        for tbl, ios in zip(tables, iosizelistlist):
            query = ("select nthread,avg({0}) from {1} where iosize={{iosize}} "
                     "group by iosize,nthread".format(col["name"], tbl))
            gds.extend(query2gds(conn, query, iosize = ios,
                                 title = "{0} {1} = {{{1}}}".format(tbl, "iosize"),
                                 **plotprefdict))
        sys.stdout.write('draw : {0}\n'.format(figpath))
        gp.plot(*gds)
    gp.close()

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

    #plot_iosize_spec(dbpath, terminaltype)
    plot_nthread_spec(dbpath, terminaltype)
