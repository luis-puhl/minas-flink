SHELL=/bin/bash

# all: clean experiments/serial-matrix.log experiments/mpi-matrix.log
# all: experiments/baseline.log experiments/mfog.log
all: bin bin/reboot/tmpi
	# experiments/reference-java.log
# cluster@almoco
# experiments/mpi-test.log

.PHONY: clean bin
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
	@-mkdir -p bin/reboot out experiments 2>/dev/null
datasets/emtpyline:
	echo "" > datasets/emtpyline
bin/redis: src/modules/redis/get-model.c
	gcc -g -Wall -lm -lhiredis -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -lglib-2.0 $^ -o $@

# -------------------------- Bin Executables -----------------------------------
# CC=gcc
# CFLAGS=-I/usr/local/include -Wl,-rpath -Wl,/usr/local/lib -Wl,--enable-new-dtags -L/usr/local/lib
# LDFLAGS=-pthread -lmpi -g -Wall -lm
bin/base.o: src/base/base.c src/base/minas.c src/base/kmeans.c src/base/clustream.c
	gcc -g -Wall -lm $^ -o $@
bin/minas-mpi: src/main.c src/base/minas.c src/mpi/minas-mpi.c
	mpicc $^ -o $@ -lm -Wall -g
bin/mfog: src/modules/mfog.c bin/base.o
	mpicc -g -Wall -lm -lpthread $^ -o $@

bin/baseline: src/modules/baseline.c bin/base.o
	gcc -g -Wall -lm $^ -o $@
bin/training: src/modules/training.c bin/base.o
	mpicc -g -Wall -lm -lhiredis $^ -o $@
bin/classifier: src/modules/classifier.c src/mpi/mfog-mpi.c src/modules/modules.c bin/base.o src/modules/redis/redis-connect.c
	mpicc -g -Wall -lm -lhiredis $^ -o $@
bin/noveltyDetection: src/modules/novelty-detection.c src/modules/modules.c bin/base.o src/modules/redis/redis-connect.c
	mpicc -g -Wall -lm -lhiredis $^ -o $@

bin/hello-mpi: src/mpi/hello-mpi.c
	mpicc -g -Wall $^ -o $@
#
# -------------------------- Reboot --------------------------------------------
bin/reboot/serial: src/reboot/base.c src/reboot/main.c
	gcc -g -Wall -lm -pthread $^ -o $@
bin/reboot/eet: src/reboot/eet.c
	gcc -g -Wall -lm -pthread $^ -o $@

bin/reboot/offline: src/reboot/base.c src/reboot/offline.c
	gcc -g -Wall -lm -pthread $^ -o $@
bin/reboot/online: src/reboot/base.c src/reboot/online.c
	gcc -g -Wall -lm -pthread $^ -o $@
bin/reboot/detection: src/reboot/base.c src/reboot/detection.c
	gcc -g -Wall -lm -pthread $^ -o $@

bin/reboot/tmpi: src/reboot/base.c src/reboot/threaded-mpi.c
	mpicc -g -Wall -lm $^ -o $@
.PHONY: bin/reboot
bin/reboot: bin/reboot/serial bin/reboot/eet bin/reboot/offline bin/reboot/online bin/reboot/detection bin/reboot/tmpi

# -------------------------- Reboot Experiments --------------------------------
ds = datasets/training.csv datasets/emtpyline datasets/test.csv
experiments/reboot/serial.log: $(ds) bin/reboot/serial src/evaluation/evaluate.py
	cat datasets/training.csv datasets/emtpyline datasets/test.csv \
		| ./bin/reboot/serial > out/reboot/serial.csv 2> $@
	python3 src/evaluation/evaluate.py Mfog-Reboot-serial \
		datasets/test.csv out/reboot/serial.csv experiments/reboot/serial.png >> $@

out/reboot/offline.csv: $(ds) bin/reboot/offline
	cat datasets/training.csv | ./bin/reboot/offline > out/reboot/offline.csv 2> experiments/reboot/offline.log

experiments/reboot/split.log: $(ds) out/reboot/offline.csv bin/reboot/online bin/reboot/detection src/evaluation/evaluate.py
	cat out/reboot/offline.csv datasets/test.csv | ./bin/reboot/online \
		> out/reboot/online.csv 2> $@
	cat out/reboot/offline.csv out/reboot/online.csv | ./bin/reboot/detection \
		> out/reboot/detection.csv 2>> $@
	grep -E -v '^(Unknown|Cluster):' out/reboot/online.csv > out/reboot/split-matches.csv
	grep -E '^Unknown:' out/reboot/online.csv > out/reboot/split-unknowns.csv
	# grep -E '^Cluster:' out/reboot/online.csv > out/reboot/split-clusters.csv
	python3 src/evaluation/evaluate.py Mfog-Reboot datasets/test.csv out/reboot/split-matches.csv \
		experiments/reboot/split.png >> $@

experiments/reboot/eet.log: $(ds) out/reboot/offline.csv bin/reboot/online bin/reboot/detection bin/reboot/eet src/evaluation/evaluate.py
	echo "" > experiments/reboot/eet.log
	# mkfifo model.fifo unk.fifo online-unk.fifo
	#
	# cat out/reboot/offline.csv | bin/reboot/eet unk.fifo | bin/reboot/detection > model.fifo \
	# 	2>> experiments/reboot/eet.log &
	# cat online-unk.fifo | grep 'Unknown:' | tee out/reboot/unknowns.csv > unk.fifo &
	# cat out/reboot/offline.csv datasets/test.csv | ./bin/reboot/eet model.fifo \
	# 	| tee out/reboot/onlineInput.csv | ./bin/reboot/online \
	# 	| tee out/reboot/online-output.csv online-unk.fifo \
	# 	| grep -v -e 'Unknown:' > out/reboot-output.csv \
	# 	2>> experiments/reboot/eet.log
	#
	# rm model.fifo unk.fifo online-unk.fifo
	# python3 src/evaluation/evaluate.py Mfog-Reboot-eet datasets/test.csv out/reboot/output.csv \
	# 	experiments/reboot/eet.png >> experiments/reboot/eet.log
#
.PHONY: tmpi
tmpi: $(ds) out/reboot/offline.csv bin/reboot/tmpi experiments/reboot/tmi-n2.log experiments/reboot/tmi-n4.log
	# cat out/reboot/offline.csv datasets/test.csv | mpirun -n 2 ./bin/reboot/tmpi
out = out/reboot/tmi-n
experiments/reboot/tmi-n2.log: $(ds) out/reboot/offline.csv bin/reboot/tmpi src/evaluation/evaluate.py
	cat out/reboot/offline.csv datasets/test.csv | mpirun -n 2 ./bin/reboot/tmpi > $(out)2.csv 2> $@
	-grep -E -v '^(Unknown|Cluster):' $(out)2.csv > $(out)2-matches.csv
	-grep -E '^Unknown:' $(out)2.csv > $(out)2-unknowns.csv
	-grep -E '^Cluster:' $(out)2.csv > $(out)2-clusters.csv
	python3 src/evaluation/evaluate.py Mfog-Reboot-tmi-n2 datasets/test.csv $(out)2-matches.csv \
		experiments/reboot/tmi-n2.png >>$@
experiments/reboot/tmi-n4.log: $(ds) out/reboot/offline.csv bin/reboot/tmpi src/evaluation/evaluate.py
	cat out/reboot/offline.csv datasets/test.csv | mpirun -n 4 ./bin/reboot/tmpi > $(out)4.csv 2> $@
	-grep -E -v '^(Unknown|Cluster):' $(out)4.csv > $(out)4-matches.csv
	-grep -E '^Unknown:' $(out)4.csv > $(out)4-unknowns.csv
	-grep -E '^Cluster:' $(out)4.csv > $(out)4-clusters.csv
	python3 src/evaluation/evaluate.py Mfog-Reboot-tmi-n4 datasets/test.csv $(out)4-matches.csv \
		experiments/reboot/tmi-n4.png >>$@
#
.PHONY: experiments/reboot
experiments/reboot: experiments/reboot/serial.log experiments/reboot/split.log experiments/reboot/eet.log experiments/reboot/tmi.log

# -------------------------- Remote Pi Cluster Experiments ---------------------
.PHONY: code@almoco src.sha1 experiments/rpi
SSH = ssh -i ./secrets/id_rsa -F ./conf/ssh.config
code@almoco:
	tar cz src makefile | $(SSH) almoco "cd cloud && tar xmvzf - >/dev/null"
	$(SSH) almoco "cd cloud && make bin/reboot && scp -r ~/cloud/bin jantar:~/cloud/ && scp -r ~/cloud/bin lanche:~/cloud/"
experiments/rpi/base-time.log: bin/hello-mpi
	time mpirun -hostfile ./conf/hostsfile hostname >$@ 2>&1
	time mpirun -hostfile ./conf/hostsfile ./bin/hello-mpi >>$@ 2>&1
experiments/rpi/serial.log: experiments/reboot/serial.log
	mv $^ $@
	mv experiments/reboot/serial.png experiments/rpi/serial.png 
experiments/rpi/split.log: experiments/reboot/split.log
	mv $^ $@
	mv experiments/reboot/split.png experiments/rpi/split.png
experiments/rpi/tmi-rpi-n12.log: $(ds) out/reboot/offline.csv bin/reboot/tmpi src/evaluation/evaluate.py
	cat out/reboot/offline.csv datasets/test.csv \
		| mpirun -hostfile ./conf/hostsfile ./bin/reboot/tmpi \
		> out/reboot/tmi-rpi-n12.csv 2> $@
	grep -E -v '^(Unknown|Cluster):' out/reboot/tmi-rpi-n12.csv > out/reboot/tmi-rpi-n12-matches.csv
	grep -E '^Unknown:' out/reboot/tmi-rpi-n12.csv > out/reboot/tmi-rpi-n12-unknowns.csv
	grep -E '^Cluster:' out/reboot/tmi-rpi-n12.csv > out/reboot/tmi-rpi-n12-clusters.csv
	python3 src/evaluation/evaluate.py Mfog-Reboot-tmi-rpi-n12 datasets/test.csv out/reboot/tmi-rpi-n12-matches.csv \
		experiments/rpi/tmi-rpi-n12.png >>$@
# experiments/rpi: experiments/rpi/base-time.log experiments/rpi/serial.log experiments/rpi/split.log experiments/rpi/tmi-rpi-n12.log
experiments/rpi/reboot.log: code@almoco
	$(SSH) almoco "cd cloud && make experiments/rpi/tmi-rpi-n12.log" > $@ 2>&1
	$(SSH) almoco "tar czf ~/cloud/out/logs.tgz ~/cloud/experiments/rpi" >> $@ 2>&1
	scp almoco:~/cloud/out/logs.tgz out/logs.tgz
	tar xzf out/logs.tgz experiments/

# experiments/rpi
	# ssh almoco "cat out/reboot/offline.csv datasets/test.csv | mpirun -n 4 ./bin/reboot/tmpi \
	# 	> out/reboot/tmi-n4.csv 2> $@mpirun --path /home/pi/cloud/ --host almoco:4,jantar:4,lanche:4 hostname"
	# ssh almoco "cd cloud && mpirun almoco:4 almoco:4"
	# mkdir -p experiments/rpi
	# scp almoco:~/cloud/experiments/* experiments/rpi/

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
experiments/baseline.log: bin/baseline src/evaluation/evaluate.py conf/minas.conf datasets/emtpyline datasets/training.csv datasets/test.csv
	cat datasets/training.csv datasets/emtpyline datasets/test.csv \
		| env $$(cat conf/minas.conf | xargs) ./bin/baseline \
		> out/baseline.csv 2> $@
	python3 src/evaluation/evaluate.py Baseline datasets/test.csv out/baseline.csv $@.png >> $@
	cat $@
# Experiments: Minas with MPI and Redis
out/mfog-model.csv: bin/training conf/minas.conf datasets/emtpyline datasets/training.csv
	echo "" > experiments/mfog.log
	# cat conf/minas.conf datasets/emtpyline datasets/training.csv datasets/emtpyline | ./bin/training > $@ 2> experiments/mfog.log
	cat datasets/training.csv datasets/emtpyline | env $$(cat conf/minas.conf | xargs) ./bin/training > $@ 2>> experiments/mfog.log
#

bin/mfog-threads: src/modules/mfog-threads.c src/base/minas.c src/modules/modules.c src/base/base.c src/base/kmeans.c src/base/clustream.c src/mpi/mfog-mpi.c
	mpicc -g -Wall -lm -lpthread $^ -o $@
experiments/mfog-threads.log: bin/mfog-threads conf/minas.conf out/mfog-model.csv
	cat out/mfog-model.csv datasets/emtpyline datasets/test.csv \
		| env $$(cat conf/minas.conf | xargs) mpirun bin/mfog-threads > out/mfog-matches.csv 2>&1 | tee $@
#

experiments/mfog-serial-sansNd.log: bin/classifier conf/minas.conf datasets/emtpyline out/mfog-model-serial.csv datasets/test.csv
	cat out/mfog-model-serial.csv datasets/emtpyline datasets/test.csv \
		| env $$(cat conf/minas.conf | xargs) ./bin/classifier 2> $@ \
		> out/mfog-serial-sansNd.csv
experiments/mfog-serial.log: bin/training bin/classifier bin/noveltyDetection conf/minas.conf datasets/emtpyline out/mfog-model-serial.csv datasets/test.csv
	cat out/mfog-model-serial.csv datasets/emtpyline - datasets/test.csv \
		| env $$(cat conf/minas.conf | xargs) ./bin/classifier 2> $@ \
		| tee out/mfog-matches.csv \
		| ./bin/noveltyDetection conf/minas.conf 2>&1 | tee $@
	python3 src/evaluation/evaluate.py Mfog-Serial datasets/test.csv out/mfog-matches.csv experiments/mfog-hits.png >> $@
	cat $@
experiments/mfog.log: bin/classifier bin/noveltyDetection conf/minas.conf datasets/emtpyline out/mfog-model.csv
	./bin/noveltyDetection conf/minas.conf 2>&1 | tee $@ &
	cat out/mfog-model.csv | python3 src/modules/redis/send-model.py
	cat conf/minas.conf datasets/emtpyline datasets/test.csv | ./bin/classifier > out/mfog-matches.csv 2>> $@
	echo "" >> $@
	python3 src/evaluation/evaluate.py Mfog datasets/test.csv out/mfog-matches.csv experiments/mfog-hits.png >> $@
	cat $@
experiments/noveltyDetection.log: bin/noveltyDetection conf/minas.conf datasets/emtpyline
	cat conf/minas.conf datasets/emtpyline | ./bin/noveltyDetection # 2>&1 | tee $@ &
#

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