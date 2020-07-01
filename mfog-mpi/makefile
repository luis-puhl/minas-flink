all: clean experiments/serial-matrix.log experiments/mpi-matrix.log cluster@almoco

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
	gcc src/minas/evaluate.c src/util/loadenv.c -o target/eval -lm -Wall -g
mfog@jantar: target/mfog
	scp target/mfog jantar:/home/pi/cloud/target/

out/serial.csv: out target/minas datasets/model-clean.csv datasets/test.csv
	target/minas MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv \
		TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/serial.csv > experiments/serial.log 2>&1
out/mpi.csv: out target/mfog datasets/model-clean.csv datasets/test.csv
	mpiexec ./target/mfog MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv \
		TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/mpi.csv > experiments/mpi.log 2>&1
out/cluster.csv: mfog@jantar datasets/model-clean.csv datasets/test.csv
	mpiexec --host jantar:4,almoco:4 ./target/mfog MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv \
		TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/cluster.csv > experiments/cluster.log 2>&1

experiments/serial-matrix.log: target/eval datasets/test.csv out/serial.csv
	target/eval EXAMPLES_CSV=datasets/test.csv TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/serial.csv \
		EVALUATE_LOG=experiments/serial-matrix.log >> experiments/serial.log 2>&1
experiments/mpi-matrix.log: target/eval datasets/test.csv out/mpi.csv
	target/eval EXAMPLES_CSV=datasets/test.csv TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/mpi.csv \
		EVALUATE_LOG=experiments/mpi-matrix.log >> experiments/mpi.log 2>&1
experiments/cluster-matrix.log: out/cluster.csv datasets/model-clean.csv datasets/test.csv
	target/eval EXAMPLES_CSV=datasets/test.csv TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/cluster.csv \
		EVALUATE_LOG=experiments/cluster-matrix.log >> experiments/cluster.log 2>&1

cluster@almoco:
	scp -r src makefile almoco:~/cloud
	ssh almoco "cd cloud && make clean experiments/serial-matrix.log experiments/mpi-matrix.log experiments/cluster-matrix.log"
	mkdir -p experiments/rpi
	scp almoco:~/cloud/experiments/* experiments/rpi/
