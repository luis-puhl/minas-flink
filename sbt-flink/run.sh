# sbt assembly 2>&1 > tmpfs/log/assembly.log
time java \
    -Djava.io.tmpdir=./tmpfs/ \
    -cp 'target/scala-2.11/sbt-flink-assembly-0.1-SNAPSHOT.jar:' br.ufscar.dc.ppgcc.gsdr.minas.MinasKddCassales \
    2>&1 > tmpfs/log/MinasKddCassales.log
# cat tmpfs/out/sorted/*
# cat tmpfs/out/head-tail/*
# cat tmpfs/out/distribution/*
grep "INFO  br.ufscar.dc.ppgcc.gsdr.minas" tmpfs/log/MinasKddCassales.log
