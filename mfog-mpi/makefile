all: clean target/mfog run

CC = mpicc
CXXFLAGS = -lm -std=c1x -Wall -g 
LDFLAGS = 
# PERF = perf stat
PERF = 

# $(APPNAME): $(OBJ)
# 	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
# $(OBJDIR)/%.o: $(SRCDIR)/%$(EXT)
# 	$(CC) $(CXXFLAGS) -o $@ -c $<
install:
	mkdir out target experiments

target/mfog: mfog.c
	mpicc mfog.c -o target/mfog $(CXXFLAGS)

target/minas-mpi: minas-mpi.c
	clang-tidy minas-mpi.c
	# mpicc minas-mpi.c -lm -std=c1x -Wall -g -o target/minas-mpi
	gcc minas-mpi.c -pthread -Wl,-rpath -Wl,/usr/lib/openmpi -Wl,--enable-new-dtags -L/usr/lib/openmpi -lmpi -lm -std=c1x -Wall -g -o target/minas-mpi

run:
	mpiexec ./target/mfog

clean:
	@-rm target/*

target/minas: minas.c
	# clang-tidy minas.c
	gcc minas.c -o target/minas $(CXXFLAGS)
target/eval: evaluate.c
	gcc evaluate.c -o target/eval $(CXXFLAGS)

out/classify.csv: target/minas
	$(PERF)./target/minas > out/classify.csv 2>experiments/eval.log
experiments/eval.log: target/eval datasets/test.csv out/classify.csv
	./target/eval ./datasets/test.csv ./out/classify.csv >> experiments/eval.log 2>>experiments/eval.log

out/classify-mpi.csv: target/minas-mpi
	$(PERF)mpiexec ./target/minas-mpi > out/classify-mpi.csv 2>experiments/eval-mpi.log
experiments/eval-mpi.log: target/eval datasets/test.csv out/classify-mpi.csv
	./target/eval ./datasets/test.csv ./out/classify-mpi.csv >> experiments/eval-mpi.log 2>>experiments/eval-mpi.log

copyAlmoco:
	scp target/minas pi@almoco:/home/pi/
run@almoco: copyAlmoco runMfog
	ssh pi@almoco sh -c "/home/pi/minas datasets/model.csv datasets/test.csv > out/classify.csv"

# minas.asm: minas.c
# 	gcc minas.c -O2 -c -S -g -std=c1x -Wall -o - -masm=intel | c++filt | grep -vE '\s+\.' > minas.gcc.asm

# .c.asm:
# 	gcc $*.c -O2 -c -S -g -std=c1x -Wall -o - -masm=intel | c++filt | grep -vE '\s+\.' > $@

# Building rule for .o files and its .c/.cpp in combination with all .h
$(OBJDIR)/%.o: $(SRCDIR)/%$(EXT)
	$(CC) $(CXXFLAGS) -o $@ -c $<

watch:
	for i in 1 2 3 4 5; do make eval-mpi.log ; echo "done"; inotifywait -qre close_write . ; done

cluster:
	# mpirun [ -np X ] [ --hostfile <filename> ]  <program>
	# --map-by ppr:n:node
	mpirun -np 4 --host localhost,192.168.15.10 target/minas-mpi

cloud/minas.tar: minas.c minas.h minas-mpi.c makefile evaluate.c
	tar -cvf cloud/minas.tar minas.c minas.h minas-mpi.c makefile evaluate.c

gsdr/minas-mpi.o: minas-mpi.c
	gcc minas-mpi.c -o gsdr/minas-mpi.o -Wl,-Bsymbolic-functions -Wl,-z,relro -I/usr/include/mpich -L/usr/lib/x86_64-linux-gnu -lmpich