sbt assembly
time java -Djava.io.tmpdir=./tmpfs/ -cp 'target/scala-2.11/sbt-flink-assembly-0.1-SNAPSHOT.jar:' br.ufscar.dc.ppgcc.gsdr.minas.KMeansVector
cat tmpfs/out/sorted/*
cat tmpfs/out/head-tail/*
cat tmpfs/out/distribution/*
