SHELL=/bin/bash

all: bin bin/tmpi

# -------------------------- Datasets and lib tests ----------------------------
bin:
	@-mkdir -p bin out experiments 2>/dev/null
datasets/emtpyline:
	echo "" > datasets/emtpyline

# -------------------------- Bin Executables -----------------------------------
# CC=gcc
# CFLAGS=-I/usr/local/include -Wl,-rpath -Wl,/usr/local/lib -Wl,--enable-new-dtags -L/usr/local/lib
# LDFLAGS=-pthread -lmpi -g -Wall -lm
bin/serial: src/base.c src/main.c
	gcc -g -Wall -lm -pthread $^ -o $@

bin/offline: src/base.c src/offline.c
	gcc -g -Wall -lm -pthread $^ -o $@
bin/online: src/base.c src/online.c
	gcc -g -Wall -lm -pthread $^ -o $@
bin/detection: src/base.c src/detection.c
	gcc -g -Wall -lm -pthread $^ -o $@

bin/tmpi: src/base.c src/threaded-mpi.c
	mpicc -g -Wall -lm $^ -o $@
.PHONY: bin/reboot
bin/reboot: bin/serial bin/offline bin/online bin/detection bin/tmpi

# -------------------------- Reboot Experiments --------------------------------
ds = datasets/training.csv datasets/emtpyline datasets/test.csv
experiments/reboot/serial.log: $(ds) bin/serial src/evaluation/evaluate.py
	cat datasets/training.csv datasets/emtpyline datasets/test.csv \
		| ./bin/serial > out/reboot/serial.csv 2> $@
	python3 src/evaluation/evaluate.py Mfog-Reboot-serial \
		datasets/test.csv out/reboot/serial.csv experiments/reboot/serial.png >> $@

out/reboot/offline.csv: $(ds) bin/offline
	cat datasets/training.csv | ./bin/offline > out/reboot/offline.csv 2> experiments/reboot/offline.log

experiments/reboot/split.log: $(ds) out/reboot/offline.csv bin/online bin/detection src/evaluation/evaluate.py
	cat out/reboot/offline.csv datasets/test.csv | ./bin/online \
		> out/reboot/online.csv 2> $@
	cat out/reboot/offline.csv out/reboot/online.csv | ./bin/detection \
		> out/reboot/detection.csv 2>> $@
	grep -E -v '^(Unknown|Cluster):' out/reboot/online.csv > out/reboot/split-matches.csv
	grep -E '^Unknown:' out/reboot/online.csv > out/reboot/split-unknowns.csv
	# grep -E '^Cluster:' out/reboot/online.csv > out/reboot/split-clusters.csv
	python3 src/evaluation/evaluate.py Mfog-Reboot datasets/test.csv out/reboot/split-matches.csv \
		experiments/reboot/split.png >> $@

.PHONY: tmpi
tmpi: $(ds) out/reboot/offline.csv bin/tmpi experiments/reboot/tmi-n2.log experiments/reboot/tmi-n4.log
	# cat out/reboot/offline.csv datasets/test.csv | mpirun -n 2 ./bin/tmpi
out = out/reboot/tmi-n
experiments/reboot/tmi-n2.log: $(ds) out/reboot/offline.csv bin/tmpi src/evaluation/evaluate.py
	cat out/reboot/offline.csv datasets/test.csv | mpirun -n 2 ./bin/tmpi > $(out)2.csv 2> $@
	-grep -E -v '^(Unknown|Cluster):' $(out)2.csv > $(out)2-matches.csv
	-grep -E '^Unknown:' $(out)2.csv > $(out)2-unknowns.csv
	-grep -E '^Cluster:' $(out)2.csv > $(out)2-clusters.csv
	python3 src/evaluation/evaluate.py Mfog-Reboot-tmi-n2 datasets/test.csv $(out)2-matches.csv \
		experiments/reboot/tmi-n2.png >>$@
experiments/reboot/tmi-n4.log: $(ds) out/reboot/offline.csv bin/tmpi src/evaluation/evaluate.py
	cat out/reboot/offline.csv datasets/test.csv | mpirun -n 4 ./bin/tmpi > $(out)4.csv 2> $@
	-grep -E -v '^(Unknown|Cluster):' $(out)4.csv > $(out)4-matches.csv
	-grep -E '^Unknown:' $(out)4.csv > $(out)4-unknowns.csv
	-grep -E '^Cluster:' $(out)4.csv > $(out)4-clusters.csv
	python3 src/evaluation/evaluate.py Mfog-Reboot-tmi-n4 datasets/test.csv $(out)4-matches.csv \
		experiments/reboot/tmi-n4.png >>$@
experiments/reboot/tmi-n4-fast.log: $(ds) out/reboot/offline.csv bin/tmpi src/evaluation/evaluate.py
	cat out/reboot/offline.csv datasets/test.csv | mpirun -n 4 ./bin/tmpi 0 > $(out)4-fastest.csv 2> $@
	echo '' >> $@
	cat out/reboot/offline.csv datasets/test.csv | mpirun -n 4 ./bin/tmpi 1 > $(out)4-fast.csv 2>> $@
	echo '' >> $@
	cat out/reboot/offline.csv datasets/test.csv | mpirun -n 4 ./bin/tmpi 2 > $(out)4.csv 2>> $@
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
experiments/rpi/tmi-rpi-n12.log: $(ds) out/reboot/offline.csv bin/tmpi src/evaluation/evaluate.py
	cat out/reboot/offline.csv datasets/test.csv \
		| mpirun -hostfile ./conf/hostsfile ./bin/tmpi \
		> out/reboot/tmi-rpi-n12.csv 2> $@
	-grep -E -v '^(Unknown|Cluster):' out/reboot/tmi-rpi-n12.csv > out/reboot/tmi-rpi-n12-matches.csv
	-grep -E '^Unknown:' out/reboot/tmi-rpi-n12.csv > out/reboot/tmi-rpi-n12-unknowns.csv
	-grep -E '^Cluster:' out/reboot/tmi-rpi-n12.csv > out/reboot/tmi-rpi-n12-clusters.csv
	python3 src/evaluation/evaluate.py Mfog-Reboot-tmi-rpi-n12 datasets/test.csv out/reboot/tmi-rpi-n12-matches.csv \
		experiments/rpi/tmi-rpi-n12.png >>$@
# experiments/rpi: experiments/rpi/base-time.log experiments/rpi/serial.log experiments/rpi/split.log experiments/rpi/tmi-rpi-n12.log
experiments/rpi/reboot.log: code@almoco
	$(SSH) almoco "cd cloud && make experiments/rpi/tmi-rpi-n12.log" > $@ 2>&1
	$(SSH) almoco "tar cz ~/cloud/experiments/{rpi/tmi-rpi-n12.{log,png},reboot/tmi-n4.{log,png}}" | tar xmzf - experiments/rpi

# experiments/rpi
	# ssh almoco "cat out/reboot/offline.csv datasets/test.csv | mpirun -n 4 ./bin/tmpi \
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
