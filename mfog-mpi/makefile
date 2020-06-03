all: clean target/mfog run

target/mfog: mfog.c
	mpicc -o target/mfog mfog.c

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

copyAlmoco:
	scp target/minas pi@almoco:/home/pi/
run@almoco: copyAlmoco runMfog
	ssh pi@almoco sh -c "/home/pi/minas datasets/model.csv datasets/test.csv > out/classify.csv"
