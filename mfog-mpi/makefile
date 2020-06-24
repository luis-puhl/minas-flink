all: clean experiments/serial.log experiments/mpi.log cluster@almoco

clean:
	@-rm target/* out/* experiments/serial.log experiments/eval-mpi.log 2>/dev/null
	@-mkdir target out experiments 2>/dev/null

target:
	@-mkdir target out experiments 2>/dev/null
target/mfog: target minas-mpi.c minas-funcs.c loadenv.c
	mpicc minas-mpi.c minas-funcs.c loadenv.c -o target/mfog -lm -Wall -g
target/minas: target minas.c minas-funcs.c loadenv.c
	gcc minas.c minas-funcs.c loadenv.c -o target/minas -lm -Wall -g
target/eval: target evaluate.c loadenv.c
	gcc evaluate.c loadenv.c -o target/eval -lm -Wall -g

PERF := $(shell perf stat ls >/dev/null 2>&1; echo $$?)

out/serial.csv: out target/minas datasets/model-clean.csv datasets/test.csv
	target/minas MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/serial.csv
ifeq ($(PERF),0)
	perf stat target/minas MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/serial.csv
endif
out/mpi.csv: out target/mfog datasets/model-clean.csv datasets/test.csv
	mpiexec ./target/mfog MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/mpi.csv
ifeq ($(PERF),0)
	perf stat mpiexec ./target/mfog MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/mpi.csv
endif
experiments/serial.log: target/eval datasets/test.csv out/serial.csv
	target/eval EXAMPLES_CSV=datasets/test.csv TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/serial.csv EVALUATE_LOG=experiments/serial.log
experiments/mpi.log: target/eval datasets/test.csv out/mpi.csv
	target/eval EXAMPLES_CSV=datasets/test.csv TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/mpi.csv EVALUATE_LOG=experiments/mpi.log

mfog@jantar: target/mfog
	scp target/mfog jantar:/home/pi/cloud/target/
out/cluster.csv: mfog@jantar datasets/model-clean.csv datasets/test.csv
	mpiexec --host jantar:4,almoco:4 ./target/mfog MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/cluster.csv
experiments/cluster.log: out/cluster.csv datasets/model-clean.csv datasets/test.csv
	target/eval EXAMPLES_CSV=datasets/test.csv TIMING_LOG=experiments/timing.csv MATCHES_CSV=out/cluster.csv EVALUATE_LOG=experiments/cluster.log

cluster@almoco:
	scp *.c *.h makefile almoco:~/cloud
	ssh almoco "cd cloud && make clean experiments/serial.log experiments/mpi.log experiments/cluster.log"
	mkdir -p experiments/rpi
	scp almoco:~/cloud/experiments/* experiments/rpi/
