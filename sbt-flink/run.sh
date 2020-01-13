[ ! -f tmpfs/KDDTe5Classes_fold1_ini.csv ] && echo "Missing input files, is TMPFS complete?" && exit 1
[ ! -d tmpfs/log ] && mkdir tmpfs/log
[ ! -d tmpfs/out ] && mkdir tmpfs/out
sbt assembly
[ $? -eq 1 ] && echo "Compile fail" && exit 1
time java \
    -Djava.io.tmpdir=./tmpfs/ \
    -cp 'target/scala-2.11/sbt-flink-assembly-0.1.jar:' br.ufscar.dc.ppgcc.gsdr.minas.MinasKdd # \
    # > tmpfs/log/MinasKddCassales.log \
    # 2> tmpfs/log/MinasKddCassales.err.log
# cat tmpfs/out/sorted/*
# cat tmpfs/out/head-tail/*
# cat tmpfs/out/distribution/*
# grep "INFO  br.ufscar.dc.ppgcc.gsdr.minas" tmpfs/log/MinasKddCassales.log
# sed -n -r 's;(.+)INFO  br.ufscar.dc.ppgcc.gsdr.minas.(.+);\2;p' tmpfs/log/MinasKddCassales.log

#cat tmpfs/log/MinasKddCassales.log

# cat tmpfs/log/MinasKddCassales.err.log
# cat tmpfs/out/stream-clusters.csv/*