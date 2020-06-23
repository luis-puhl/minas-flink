all: clean experiments/eval.log experiments/eval-mpi.log

install:
	mkdir -p out target $$EXPE
clean:
	@-rm target/* out/* experiments/eval.log experiments/eval-mpi.log

target/mfog: minas-mpi.c minas-funcs.c loadenv.c
	# gcc mfog.c -pthread -Wl,-rpath -Wl,/usr/lib/openmpi -Wl,--enable-new-dtags -L/usr/lib/openmpi -lmpi -lm -std=c1x -Wall -g -o target/mfog
	mpicc minas-mpi.c minas-funcs.c loadenv.c -o target/mfog -lm -Wall -g
target/minas: minas.c minas-funcs.c loadenv.c
	gcc minas.c minas-funcs.c loadenv.c -o target/minas -lm -Wall -g
target/eval: evaluate.c loadenv.c
	gcc evaluate.c loadenv.c -o target/eval -lm -Wall -g

targets: target/mfog target/minas target/eval

.EXPORT_ALL_VARIABLES:

MFOG_MODEL_CSV=datasets/model-clean.csv
MFOG_EXAMPLES_CSV=datasets/test.csv
MFOG_MATCHES_CSV=out/classify.csv
MFOG_TIMING_LOG=experiments/timing.csv

# MODEL_CSV=datasets/model-clean.csv EXAMPLES_CSV=datasets/test.csv MATCHES_CSV=out/classify.csv TIMING_LOG=experiments/timing.csv

PERF := $(shell perf stat ls 1>&2 2> /dev/null; echo $$?)
out/classify.csv: target/minas datasets/model-clean.csv datasets/test.csv
ifeq ($(PERF),0)
	perf stat target/minas 2>>experiments/eval.log
else
	./target/minas > out/classify.csv 2>experiments/eval.log
endif
out/classify-mpi.csv: target/mfog datasets/model-clean.csv datasets/test.csv
ifeq ($(PERF),0)
	perf stat mpiexec ./target/mfog MATCHES_CSV=out/classify-mpi.csv 2>>experiments/eval-mpi.log
else
	mpiexec ./target/mfog MATCHES_CSV=out/classify-mpi.csv 2>>experiments/eval-mpi.log
endif
experiments/eval.log: target/eval datasets/test.csv out/classify.csv
	target/eval EVALUATE_LOG=experiments/eval.log
experiments/eval-mpi.log: target/eval datasets/test.csv out/classify-mpi.csv
	target/eval MATCHES_CSV=out/classify-mpi.csv EVALUATE_LOG=experiments/eval-mpi.log

out/cluster.csv: target/minas datasets/model-clean.csv datasets/test.csv
	mpirun --host almoco:4,jantar:4 $$PWD/target/mfog \
		MODEL_CSV=datasets/model-clean.csv \
		EXAMPLES_CSV=datasets/test.csv \
		MATCHES_CSV=out/cluster.csv \
		TIMING_LOG=experiments/timing.csv >> experiments/cluster.log
experiments/cluster.log: target/eval datasets/test.csv out/cluster.csv
	target/eval MATCHES_CSV=out/cluster.csv EVALUATE_LOG=experiments/cluster.log
	# target/eval EXAMPLES_CSV=datasets/test.csv MATCHES_CSV=out/cluster.csv EVALUATE_LOG=experiments/cluster.log TIMING_LOG=stdout

cluster:
	mpiexec --host jantar:4,almoco:4 ./target/mfog \
		MODEL_CSV=datasets/model-clean.csv \
		EXAMPLES_CSV=datasets/test.csv \
		MATCHES_CSV=out/classify-mpi-cluster.csv \
		TIMING_LOG=experiments/timing.csv \
		2>>experiments/eval-mpi-cluster.log
	# mpirun [ -np X ] [ --hostfile <filename> ]  <program>
	# --map-by ppr:n:node
	# mpirun -np 4 --host localhost,192.168.15.10 target/mfog
	# usr/local/bin/mpiexec --prefix /home/pi/usr/local/ --host 192.168.15.10 ls
	usr/local/bin/mpiexec --prefix /home/pi/usr/local/  -np 4 --host localhost,192.168.15.10 target/mfog
cloud/minas.tar: minas.c minas.h mfog.c makefile evaluate.c
	tar -cvf cloud/minas.tar minas.c minas.h mfog.c makefile evaluate.c
copyAlmoco:
	scp target/minas pi@almoco:/home/pi/
