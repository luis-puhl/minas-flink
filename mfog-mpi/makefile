all: clean experiments/eval.log experiments/eval-mpi.log

install:
	mkdir -p out target $$EXPE
clean:
	@-rm target/* 

target/mfog: minas-mpi.c minas-funcs.c
	# gcc mfog.c -pthread -Wl,-rpath -Wl,/usr/lib/openmpi -Wl,--enable-new-dtags -L/usr/lib/openmpi -lmpi -lm -std=c1x -Wall -g -o target/mfog
	mpicc minas-mpi.c minas-funcs.c -o target/mfog -lm -Wall -g
target/minas: minas.c
	gcc minas.c minas-funcs.c -o target/minas -lm -Wall -g
target/eval: evaluate.c
	gcc evaluate.c -o target/eval -lm -Wall -g

targets: target/mfog target/minas target/eval

out/classify.csv: target/minas datasets/model-clean.csv datasets/test.csv
	# perf stat ./target/minas > out/classify.csv 2>experiments/eval.log
	./target/minas > out/classify.csv 2>experiments/eval.log
out/classify-mpi.csv: target/mfog datasets/model-clean.csv datasets/test.csv
	# perf stat mpiexec ./target/mfog > out/classify-mpi.csv 2>experiments/eval-mpi.log
	mpiexec ./target/mfog > out/classify-mpi.csv 2>experiments/eval-mpi.log
experiments/eval.log: target/eval datasets/test.csv out/classify.csv
	./target/eval datasets/test.csv out/classify.csv >> experiments/eval.log 2>&1
experiments/eval-mpi.log: target/eval datasets/test.csv out/classify-mpi.csv
	./target/eval datasets/test.csv out/classify-mpi.csv >> experiments/eval-mpi.log 2>&1

cluster:
	# mpirun [ -np X ] [ --hostfile <filename> ]  <program>
	# --map-by ppr:n:node
	mpirun -np 4 --host localhost,192.168.15.10 target/mfog
cloud/minas.tar: minas.c minas.h mfog.c makefile evaluate.c
	tar -cvf cloud/minas.tar minas.c minas.h mfog.c makefile evaluate.c
copyAlmoco:
	scp target/minas pi@almoco:/home/pi/
