all: clean target/mfog run

target/mfog: mfog.c
	mpicc -o target/mfog mfog.c

run:
	mpiexec ./target/mfog

clean:
	@-rm target/*

target/minas: minas.c
	gcc -lm -std=c2x -Wall minas.c -o target/minas
runMinas:
	./target/minas datasets/model.csv datasets/test.csv > out/classify.csv
