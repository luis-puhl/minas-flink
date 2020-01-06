[ ! -d tmpfs/log ] && echo "Missing log dir" && exit 1
[ ! -d tmpfs/out ] && echo "Missing out dir" && exit 1
sbt assembly && \
time java \
    -Djava.io.tmpdir=./tmpfs/ \
    -cp 'target/scala-2.11/sbt-flink-assembly-0.1-SNAPSHOT.jar:' br.ufscar.dc.ppgcc.gsdr.minas.MinasKddCassales \
    > tmpfs/log/MinasKddCassales.log \
    2> tmpfs/log/MinasKddCassales.err.log
# cat tmpfs/out/sorted/*
# cat tmpfs/out/head-tail/*
# cat tmpfs/out/distribution/*
# grep "INFO  br.ufscar.dc.ppgcc.gsdr.minas" tmpfs/log/MinasKddCassales.log
sed -n -r 's;(.+)INFO  br.ufscar.dc.ppgcc.gsdr.minas.(.+);\2;p' tmpfs/log/MinasKddCassales.log
# cat tmpfs/log/MinasKddCassales.err.log
cat tmpfs/out/stream-clusters.csv/*