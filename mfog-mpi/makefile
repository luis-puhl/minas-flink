all: clean target/mfog run

CC = mpicc
CXXFLAGS = -lm -std=c2x -Wall -g 
LDFLAGS = 

# $(APPNAME): $(OBJ)
# 	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
# $(OBJDIR)/%.o: $(SRCDIR)/%$(EXT)
# 	$(CC) $(CXXFLAGS) -o $@ -c $<

target/mfog: mfog.c
	mpicc -lm -std=c2x -Wall -g -o target/mfog mfog.c

target/minas-mpi: minas-mpi.c
	mpicc -lm -std=c2x -Wall -g -o target/minas-mpi minas-mpi.c

run:
	mpiexec ./target/mfog

clean:
	@-rm target/*

target/minas: minas.c
	gcc -lm -std=c2x -Wall -g minas.c -o target/minas
target/eval: evaluate.c
	gcc -lm -std=c2x -Wall -g evaluate.c -o target/eval

out/classify.csv: target/minas datasets/model.csv datasets/test.csv
	./target/minas datasets/model.csv datasets/test.csv > out/classify.csv 2>eval.log
eval.log: target/eval datasets/test.csv out/classify.csv
	./target/eval ./datasets/test.csv ./out/classify.csv >> eval.log 2>>eval.log

out/classify-mpi.csv: target/minas-mpi datasets/model.csv datasets/test.csv
	mpiexec ./target/minas-mpi datasets/model.csv datasets/test.csv #> out/classify-mpi.csv 2>eval-mpi.log
eval-mpi.log: target/eval datasets/test.csv out/classify-mpi.csv
	./target/eval ./datasets/test.csv ./out/classify.csv >> eval-mpi.log 2>>eval-mpi.log

copyAlmoco:
	scp target/minas pi@almoco:/home/pi/
run@almoco: copyAlmoco runMfog
	ssh pi@almoco sh -c "/home/pi/minas datasets/model.csv datasets/test.csv > out/classify.csv"
