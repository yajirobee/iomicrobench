#! /bin/bash

user=`whoami`
date=`date '+%Y%m%d%H%M%S'`

exename=`basename $1`
tmp=`basename $2`
fname=${tmp%.*}
resname=$exename\_$fname\_$3
datadir=/data/local/keisuke/storageperf/${date}

ioproffile=${datadir}/${resname}.io
cpuproffile=${datadir}/${resname}.cpu
resultfile=${datadir}/${resname}.res
topfile=${datadir}/${resname}.top

echo "execute : $@"
mkdir -p ${datadir}
iostat -x 1 > ${ioproffile} &
mpstat -P ALL 1 > ${cpuproffile} &
#top -b -d 5 > ${topfile} &
(time $@) > ${resultfile} 2>&1
#pkill -f top -U ${user}
pkill -f iostat -U ${user}
pkill -f mpstat -U ${user}
