all: clean experiments/serial-matrix.log experiments/mpi-matrix.log
# cluster@almoco
# experiments/mpi-test.log

clean:
	@-rm target/* out/* experiments/*.log 2>/dev/null
	@-mkdir target out experiments 2>/dev/null

target:
	@-mkdir target out experiments 2>/dev/null
target/mfog: target src/main.c src/minas/minas.c src/util/loadenv.c src/mpi/minas-mpi.c
	mpicc src/main.c src/minas/minas.c src/util/loadenv.c src/mpi/minas-mpi.c -o target/mfog -lm -Wall -g
target/minas: target src/main.c src/minas/minas.c src/util/loadenv.c src/mpi/minas-mpi.c
	mpicc src/main.c src/minas/minas.c src/util/loadenv.c src/mpi/minas-mpi.c -o target/minas -lm -Wall -g
target/eval: target src/minas/evaluate.c src/util/loadenv.c
	gcc src/minas/evaluate.c src/minas/minas.c src/util/loadenv.c -o target/eval -lm -Wall -g
target/mpi-test: target src/util/mpi-test.c
	mpicc src/util/mpi-test.c -o target/mpi-test -lm -Wall -g
	cp target/mpi-test /home/pi/cloud/target/
	scp src/util/mpi-test.c almoco:~/cloud/src/util/mpi-test.c
	ssh almoco "mpicc ~/cloud/src/util/mpi-test.c -o ~/cloud/target/mpi-test -lm -Wall -g"
	scp almoco:~/cloud/target/mpi-test jantar:~/cloud/target/mpi-test
	echo "build complete\n"

out/serial.csv: out target/minas datasets/model-clean.csv datasets/test.csv
	target/minas MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv \
		TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/serial.csv > experiments/serial.log 2>&1
out/serial-sorted.csv: out/serial.csv
	sort --field-separator=',' --key=1 -n out/serial.csv > out/serial-sorted.csv
out/mpi.csv: out target/mfog datasets/model-clean.csv datasets/test.csv
	mpiexec ./target/mfog MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv \
		TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/mpi.csv > experiments/mpi.log 2>&1
out/mpi-sorted.csv: out/mpi.csv
	sort --field-separator=',' --key=1 -n out/mpi.csv > out/mpi-sorted.csv
out/cluster.csv: datasets/model-clean.csv datasets/test.csv
	scp target/mfog jantar:/home/pi/cloud/target/
	scp target/mfog lanche:/home/pi/cloud/target/
	mpiexec --host jantar:4,almoco:4,lanche:4 ./target/mfog MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv \
		TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/cluster.csv > experiments/cluster.log 2>&1
out/cluster-sorted.csv: out/cluster.csv
	sort --field-separator=',' --key=1 -n out/cluster.csv > out/cluster-sorted.csv

# diffObjs = experiments/mpi-serial.diff experiments/cluster-serial.diff
# $(diffObjs): experiments/%-serial.diff: out/%-sorted.csv out/serial-sorted.csv
# 	diff out/serial-sorted.csv $< > $@

experiments/serial-matrix.log: datasets/test.csv out/serial.csv
	python3 src/minas/evaluate.py datasets/test.csv out/serial.csv > $@
experiments/mpi-matrix.log: datasets/test.csv out/mpi.csv out/serial-sorted.csv out/mpi-sorted.csv
	python3 src/minas/evaluate.py datasets/test.csv out/mpi.csv > $@
	@-echo 'diff with serial===' >> $@
	diff out/serial-sorted.csv out/mpi-sorted.csv >> $@
	@-echo '===diff with serial' >> $@
experiments/cluster-matrix.log: out/cluster-sorted.csv datasets/test.csv
	python3 src/minas/evaluate.py datasets/test.csv out/cluster-sorted.csv > $@
	@-echo 'diff with serial===' >> $@
	diff out/serial-sorted.csv out/cluster-sorted.csv >> $@
	@-echo '===diff with serial' >> $@
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

code@almoco:
	scp -r src makefile almoco:~/cloud
serial-mpi@almoco: code@almoco
	ssh almoco "cd cloud && make clean experiments/serial-matrix.log experiments/mpi-matrix.log"
	mkdir -p experiments/rpi
	scp almoco:~/cloud/experiments/* experiments/rpi/
cluster@almoco: code@almoco
	ssh almoco "cd cloud && make experiments/cluster-matrix.log"
	mkdir -p experiments/rpi
	scp almoco:~/cloud/experiments/* experiments/rpi/
