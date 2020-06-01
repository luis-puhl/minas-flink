all: clean target/mfog run

target/mfog: mfog.c
	mpicc -o target/mfog mfog.c

run:
	mpiexec ./target/mfog

clean:
	@-rm target/*

target/minas:
	gcc -lm -std=c2x -Wall minas.c -o target/minas
	./target/minas datasets/model.csv datasets/test.csv
