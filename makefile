# all: clean experiments/serial-matrix.log experiments/mpi-matrix.log
all: experiments/reference-java.log experiments/baseline.log experiments/mfog.log
# cluster@almoco
# experiments/mpi-test.log

.PHONY: clean
clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 
	@-rm bin/* experiments/*.log 2>/dev/null
	@-mkdir bin out experiments 2>/dev/null
	@echo clean
clean-mfog:
	@-rm out/baseline-model.csv experiments/mfog.log experiments/mfog-hits.png
	redis-cli FLUSHALL

# -------------------------- Datasets and lib tests ----------------------------
bin:
	@-mkdir bin out experiments 2>/dev/null
datasets/emtpyline:
	echo "" > datasets/emtpyline
bin/redis: src/modules/redis/get-model.c
	gcc -g -Wall -lm -lhiredis -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -lglib-2.0 $^ -o $@

# -------------------------- Bin Executables -----------------------------------
bin/minas-mpi: src/main.c src/base/minas.c src/mpi/minas-mpi.c
	mpicc $^ -o $@ -lm -Wall -g
bin/mfog-threads: src/modules/mfog-threads.c src/base/minas.c src/base/base.c src/base/kmeans.c src/base/clustream.c
	gcc -g -Wall -lm -lpthread $^ -o $@

bin/baseline: src/modules/baseline.c src/base/minas.c src/base/base.c src/base/kmeans.c src/base/clustream.c
	gcc -g -Wall -lm $^ -o $@
bin/training: src/modules/training.c src/base/minas.c src/base/base.c src/base/kmeans.c src/base/clustream.c
	mpicc -g -Wall -lm -lhiredis $^ -o $@
bin/classifier: src/modules/classifier.c src/mpi/mfog-mpi.c src/modules/modules.c src/base/minas.c src/base/base.c src/base/kmeans.c src/base/clustream.c src/modules/redis/redis-connect.c
	mpicc -g -Wall -lm -lhiredis $^ -o $@
bin/noveltyDetection: src/modules/novelty-detection.c src/modules/modules.c src/base/minas.c src/base/base.c src/base/kmeans.c src/base/clustream.c src/modules/redis/redis-connect.c
	mpicc -g -Wall -lm -lhiredis $^ -o $@

# -------------------------- Experiments ---------------------------------------
# --------- Experiments: Java reference ---------
experiments/reference-java.log: bin/minas/src-minas.jar datasets/training.csv datasets/test.csv
	java -classpath 'bin/minas/src-minas.jar:' br.ufu.noveltydetection.minas.Minas \
		datasets/training.csv datasets/test.csv out/minas-og/ \
		kmeans kmeans \
		2.0 1 10000 100 true | tee -a $@
experiments/reference-java.log.png: experiments/reference-java.log out/minas-og/2020-08-25T12-18-16.272/results
	python3 src/evaluation/evaluate.py Reference-Java datasets/test.csv out/minas-og/2020-08-25T12-18-16.272/results $@ >> experiments/reference-java.log
experiments/reference-java-nf.log: bin/minas/src-minas-mfogFormat.jar datasets/training.csv datasets/test.csv
	java -ea -classpath 'bin/minas/src-minas-mfogFormat.jar:' br.ufu.noveltydetection.minas.MinasOg \
		datasets/training.csv datasets/test.csv out/minas-nf/ \
		kmeans kmeans \
		2.0 lit 10000 100 false false > $@
experiments/reference-java-nf.log.png: experiments/reference-java-nf.log out/minas-nf/2020-10-05T15-55-37.147/results
	python3 src/evaluation/evaluate.py Reference-Java datasets/test.csv out/minas-nf/2020-10-05T15-55-37.147/results $@ >> experiments/reference-java-nf.log
#

# --------- Experiments: Baseline ---------
experiments/baseline.log: bin/baseline src/evaluation/evaluate.py minas.conf datasets/emtpyline datasets/training.csv datasets/test.csv
	cat datasets/training.csv datasets/emtpyline datasets/test.csv \
		| env $$(cat minas.conf | xargs) ./bin/baseline \
		> out/baseline.csv 2> $@
	python3 src/evaluation/evaluate.py Baseline datasets/test.csv out/baseline.csv $@.png >> $@
	cat $@
# Experiments: Minas with MPI and Redis
out/mfog-model.csv: bin/training minas.conf datasets/emtpyline datasets/training.csv
	echo "" > experiments/mfog.log
	# cat minas.conf datasets/emtpyline datasets/training.csv datasets/emtpyline | ./bin/training > $@ 2> experiments/mfog.log
	cat datasets/training.csv datasets/emtpyline | env $$(cat minas.conf | xargs) ./bin/training > $@ 2>> experiments/mfog.log
experiments/mfog-serial-sansNd.log: bin/classifier minas.conf datasets/emtpyline out/mfog-model-serial.csv datasets/test.csv
	cat out/mfog-model-serial.csv datasets/emtpyline datasets/test.csv \
		| env $$(cat minas.conf | xargs) ./bin/classifier 2> $@ \
		> out/mfog-serial-sansNd.csv
experiments/mfog-serial.log: bin/training bin/classifier bin/noveltyDetection minas.conf datasets/emtpyline out/mfog-model-serial.csv datasets/test.csv
	cat out/mfog-model-serial.csv datasets/emtpyline - datasets/test.csv \
		| env $$(cat minas.conf | xargs) ./bin/classifier 2> $@ \
		| tee out/mfog-matches.csv \
		| ./bin/noveltyDetection minas.conf 2>&1 | tee $@
	python3 src/evaluation/evaluate.py Mfog-Serial datasets/test.csv out/mfog-matches.csv experiments/mfog-hits.png >> $@
	cat $@
experiments/mfog.log: bin/classifier bin/noveltyDetection minas.conf datasets/emtpyline out/mfog-model.csv
	./bin/noveltyDetection minas.conf 2>&1 | tee $@ &
	cat out/mfog-model.csv | python3 src/modules/redis/send-model.py
	cat minas.conf datasets/emtpyline datasets/test.csv | ./bin/classifier > out/mfog-matches.csv 2>> $@
	echo "" >> $@
	python3 src/evaluation/evaluate.py Mfog datasets/test.csv out/mfog-matches.csv experiments/mfog-hits.png >> $@
	cat $@
experiments/noveltyDetection.log: bin/noveltyDetection minas.conf datasets/emtpyline
	cat minas.conf datasets/emtpyline | ./bin/noveltyDetection # 2>&1 | tee $@ &

# -------------------------- Remote Pi Cluster Experiments ---------------------
code@almoco:
	scp -r src makefile almoco:~/cloud
	ssh almoco "cd cloud && make"
	scp almoco:/home/pi/cloud/bin/minas-mpi jantar:/home/pi/cloud/bin/
	scp almoco:/home/pi/cloud/bin/minas-mpi lanche:/home/pi/cloud/bin/
cluster@almoco: code@almoco
	ssh almoco "cd cloud && make experiments/cluster-matrix.log"
	mkdir -p experiments/rpi
	scp almoco:~/cloud/experiments/* experiments/rpi/

# diffObjs = experiments/mpi-serial.diff experiments/cluster-serial.diff
# $(diffObjs): experiments/%-serial.diff: out/%-sorted.csv out/serial-sorted.csv
# 	diff out/serial-sorted.csv $< > $@

# experiments/serial-matrix.log: datasets/test.csv out/serial.csv
# 	python3 src/evaluation/evaluate.py datasets/test.csv out/serial.csv > $@
# experiments/mpi-matrix.log: datasets/test.csv out/mpi.csv
# 	python3 src/evaluation/evaluate.py datasets/test.csv out/mpi.csv \
# 		out/og/2020-07-20T12-18-21.758/matches.csv datasets/model-clean.csv \
# 		out/model.csv > $@
# 	sort --field-separator=',' --key=1 -n out/serial.csv > out/serial-sorted.csv
# 	sort --field-separator=',' --key=1 -n out/mpi.csv > out/mpi-sorted.csv
# 	@-echo 'diff with serial===' >> $@
# 	diff out/serial-sorted.csv out/mpi-sorted.csv -q >> $@
# 	diff out/serial-sorted.csv out/mpi-sorted.csv | head >> $@
# 	@-echo '===diff with serial' >> $@
# experiments/cluster-matrix.log: datasets/test.csv out/cluster.csv
# 	python3 src/evaluation/evaluate.py datasets/test.csv out/cluster.csv > $@
# 	sort --field-separator=',' --key=1 -n out/serial.csv > out/serial-sorted.csv
# 	sort --field-separator=',' --key=1 -n out/cluster.csv > out/cluster-sorted.csv
# 	@-echo 'diff with serial===' >> $@
# 	diff out/serial-sorted.csv out/cluster-sorted.csv -q >> $@
# 	diff out/serial-sorted.csv out/cluster-sorted.csv | head >> $@
# 	@-echo '===diff with serial' >> $@

# experiments/noveltyDetection.log: bin/mfog src/evaluation/evaluate.py
# 	bin/mfog TRAINING_CSV=datasets/training.csv MODEL_CSV=datasets/model-clean.csv \
# 		EXAMPLES_CSV=datasets/test.csv MATCHES_CSV out/matches.csv > $@
# 	python3 src/evaluation/evaluate.py >> $@

# intellij args
# datasets/KDD/KDDTe5Classes_fold1_ini.csv datasets/KDD/KDDTe5Classes_fold1_onl.csv out/KDD clustream clustream 2.0 1 10000 100 true

# target/mpi-test: target src/util/mpi-test.c
# 	mpicc src/util/mpi-test.c -o target/mpi-test -lm -Wall -g
# 	cp target/mpi-test /home/pi/cloud/target/
# 	scp src/util/mpi-test.c almoco:~/cloud/src/util/mpi-test.c
# 	ssh almoco "mpicc ~/cloud/src/util/mpi-test.c -o ~/cloud/target/mpi-test -lm -Wall -g"
# 	scp almoco:~/cloud/target/mpi-test jantar:~/cloud/target/mpi-test
# 	echo "build complete\n"
# experiments/mpi-test.log: target/mpi-test
# 	echo "" > experiments/mpi-test.log
# 	echo "$$ mpiexec --host localhost:2 /home/pi/cloud/target/mpi-test" >> experiments/mpi-test.log
# 	mpiexec --host localhost:2 /home/pi/cloud/target/mpi-test >> experiments/mpi-test.log 2>&1
# 	# 
# 	echo "" >> experiments/mpi-test.log
# 	echo "$$ mpiexec --host almoco:2 /home/pi/cloud/target/mpi-test" >> experiments/mpi-test.log
# 	mpiexec --host almoco:2 /home/pi/cloud/target/mpi-test >> experiments/mpi-test.log 2>&1
# 	# 
# 	echo "" >> experiments/mpi-test.log
# 	echo "$$ mpiexec --host localhost:1,almoco:1 /home/pi/cloud/target/mpi-test" >> experiments/mpi-test.log
# 	mpiexec --host localhost:1,almoco:1 /home/pi/cloud/target/mpi-test >> experiments/mpi-test.log 2>&1

# -------------------------- Bin Executables -----------------------------------
# IDIR =../include
# CFLAGS=-I$(IDIR)
# LIBS=-lm

# ODIR=obj
# LDIR =../lib

# _DEPS = hellomake.h
# DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

# _OBJ = hellomake.o hellofunc.o 
# OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
# $(ODIR)/%.o: %.c $(DEPS)
# 	$(CC) -c -o $@ $< $(CFLAGS)

# hellomake: $(OBJ)
# 	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)
#