all: clean target/mfog run

target/mfog: mfog.c
	mpicc -o target/mfog mfog.c

run:
	mpiexec ./target/mfog

clean:
	@-rm target/mfog