all: clean experiments/serial-matrix.log experiments/mpi-matrix.log
# cluster@almoco
# experiments/mpi-test.log

clean:
	@-rm target/* experiments/*.log 2>/dev/null
	@-mkdir target out experiments 2>/dev/null
	@echo clean

target:
	@-mkdir target out experiments 2>/dev/null
target/mfog: src/main.c src/minas/minas.c src/minas/nd-service.c src/util/loadenv.c src/util/kMeans.c src/util/net.c src/mpi/minas-mpi.c
	mpicc src/main.c src/minas/minas.c src/minas/nd-service.c src/util/loadenv.c src/util/kMeans.c src/util/net.c src/mpi/minas-mpi.c -o $@ -lm -Wall -g
code@almoco:
	scp -r src makefile almoco:~/cloud
	ssh almoco "cd cloud && make"
	scp almoco:/home/pi/cloud/target/mfog jantar:/home/pi/cloud/target/
	scp almoco:/home/pi/cloud/target/mfog lanche:/home/pi/cloud/target/

out/serial.csv: out target/mfog datasets/model-clean.csv datasets/test.csv
	./target/mfog k=100 dimension=22 MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv \
		TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/serial.csv > experiments/serial.log 2>&1
out/mpi.csv: out target/mfog datasets/model-clean.csv datasets/test.csv
	mpiexec ./target/mfog k=100 dimension=22 MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv \
		TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/mpi.csv > experiments/mpi.log 2>&1
out/cluster.csv: datasets/model-clean.csv datasets/test.csv
	mpiexec --host jantar:4,almoco:4,lanche:4 ./target/mfog k=100 dimension=22 MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv \
		TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/cluster.csv > experiments/cluster.log 2>&1

# diffObjs = experiments/mpi-serial.diff experiments/cluster-serial.diff
# $(diffObjs): experiments/%-serial.diff: out/%-sorted.csv out/serial-sorted.csv
# 	diff out/serial-sorted.csv $< > $@

experiments/serial-matrix.log: datasets/test.csv out/serial.csv
	python3 src/minas/evaluate.py datasets/test.csv out/serial.csv > $@
experiments/mpi-matrix.log: datasets/test.csv out/mpi.csv
	python3 src/minas/evaluate.py datasets/test.csv out/mpi.csv \
		out/og/2020-07-20T12-18-21.758/matches.csv datasets/model-clean.csv \
		out/model.csv > $@
	sort --field-separator=',' --key=1 -n out/serial.csv > out/serial-sorted.csv
	sort --field-separator=',' --key=1 -n out/mpi.csv > out/mpi-sorted.csv
	@-echo 'diff with serial===' >> $@
	diff out/serial-sorted.csv out/mpi-sorted.csv -q >> $@
	diff out/serial-sorted.csv out/mpi-sorted.csv | head >> $@
	@-echo '===diff with serial' >> $@
experiments/cluster-matrix.log: datasets/test.csv out/cluster.csv
	python3 src/minas/evaluate.py datasets/test.csv out/cluster.csv > $@
	sort --field-separator=',' --key=1 -n out/serial.csv > out/serial-sorted.csv
	sort --field-separator=',' --key=1 -n out/cluster.csv > out/cluster-sorted.csv
	@-echo 'diff with serial===' >> $@
	diff out/serial-sorted.csv out/cluster-sorted.csv -q >> $@
	diff out/serial-sorted.csv out/cluster-sorted.csv | head >> $@
	@-echo '===diff with serial' >> $@

experiments/noveltyDetection.log: target/mfog src/minas/evaluate.py
	target/mfog TRAINING_CSV=datasets/training.csv MODEL_CSV=datasets/model-clean.csv \
		EXAMPLES_CSV=datasets/test.csv MATCHES_CSV out/matches.csv > $@
	python3 src/minas/evaluate.py >> $@

cluster@almoco: code@almoco
	ssh almoco "cd cloud && make experiments/cluster-matrix.log"
	mkdir -p experiments/rpi
	scp almoco:~/cloud/experiments/* experiments/rpi/

runJava:
	java -classpath '../ref-git/src-minas/out/artifacts/src_minas_jar/src-minas.jar:' br.ufu.noveltydetection.minas.Minas \
		datasets/training.csv datasets/test.csv out/KDD/ \
		clustream clustream \
		2.0 1 10000 100 true
	# intellij args
	# datasets/KDD/KDDTe5Classes_fold1_ini.csv datasets/KDD/KDDTe5Classes_fold1_onl.csv out/KDD clustream clustream 2.0 1 10000 100 true

# target/mpi-test: target src/util/mpi-test.c
# 	mpicc src/util/mpi-test.c -o target/mpi-test -lm -Wall -g
# 	cp target/mpi-test /home/pi/cloud/target/
# 	scp src/util/mpi-test.c almoco:~/cloud/src/util/mpi-test.c
# 	ssh almoco "mpicc ~/cloud/src/util/mpi-test.c -o ~/cloud/target/mpi-test -lm -Wall -g"
# 	scp almoco:~/cloud/target/mpi-test jantar:~/cloud/target/mpi-test
# 	echo "build complete\n"
# experiments/mpi-test.log: target/mpi-test
# 	echo "" > experiments/mpi-test.log
# 	echo "$$ mpiexec --host localhost:2 /home/pi/cloud/target/mpi-test" >> experiments/mpi-test.log
# 	mpiexec --host localhost:2 /home/pi/cloud/target/mpi-test >> experiments/mpi-test.log 2>&1
# 	# 
# 	echo "" >> experiments/mpi-test.log
# 	echo "$$ mpiexec --host almoco:2 /home/pi/cloud/target/mpi-test" >> experiments/mpi-test.log
# 	mpiexec --host almoco:2 /home/pi/cloud/target/mpi-test >> experiments/mpi-test.log 2>&1
# 	# 
# 	echo "" >> experiments/mpi-test.log
# 	echo "$$ mpiexec --host localhost:1,almoco:1 /home/pi/cloud/target/mpi-test" >> experiments/mpi-test.log
# 	mpiexec --host localhost:1,almoco:1 /home/pi/cloud/target/mpi-test >> experiments/mpi-test.log 2>&1

