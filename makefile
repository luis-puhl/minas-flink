SHELL=/bin/bash

.PHONY: all clean
all: experiments/online-nd.log experiments/tmi-n experiments/tmi-supressed.log experiments/tmi-classifiers
clean:
	-rm bin/offline bin/ond bin/tmpi 
	-rm out/*.csv
	-rm experiments/*.{log,png}

# -------------------------- Bin Executables -----------------------------------
bin/offline: src/base.c src/offline.c
	gcc -g -Wall -lm -pthread $^ -o $@
bin/ond: src/base.c src/online-nd.c
	gcc -g -Wall -lm $^ -o $@
bin/tmpi: src/base.c src/threaded-mpi.c
	mpicc -g -Wall -lm $^ -o $@

bin/fk: src/base.c src/partials/classifier.c
	gcc -g -Wall -lm $^ -o $@
bin/dt: src/base.c src/partials/detection.c
	gcc -g -Wall -lm $^ -o $@
bin/zip: src/partials/zip.c
	gcc -g -Wall $^ -o $@
bin/rb: src/partials/robin.c
	gcc -g -Wall $^ -o $@

.PHONY: bin
bin: bin/offline bin/ond bin/tmpi bin/fk bin/dt bin/zip

# --------------------------------- Experiments --------------------------------
export TIME_FORMAT="%C\n\t%U user\t%S system\t%E elapsed\n\t%P CPU\t(%X avgtext+%D avgdata\t%M maxresident)k\n\t%I inputs+%O outputs\t(%F major+%R minor)pagefaults\t%W swaps\n"
TIME := /usr/bin/time --format="$$TIME_FORMAT" /usr/bin/time --output=experiments/timing.log --append --format="$$TIME_FORMAT"
out/offline-model.csv: datasets/training.csv bin/offline
	cat datasets/training.csv | $(TIME) ./bin/offline > out/offline-model.csv 2> experiments/offline-model.log

experiments/online-nd.log: out/offline-model.csv datasets/test.csv bin/ond
	cat out/offline-model.csv datasets/test.csv | $(TIME) ./bin/ond > out/ond-0full.csv 2> $@
	-grep -E -v '^(Unknown|Cluster):' out/ond-0full.csv > out/ond-1matches.csv
	-python3 src/evaluation/evaluate.py "Serial Online-ND" datasets/test.csv out/ond-1matches.csv $@.png >> $@

ds = datasets/training.csv datasets/emtpyline datasets/test.csv
n = $(subst experiments/tmi-n,,$(subst .log,,$@))
out = $(subst experiments/,out/,$(subst .log,,$@))
tmpi_nodes := $(foreach wrd,2 3 4,experiments/tmi-n$(wrd).log)
experiments/tmi-n: $(tmpi_nodes)
.PHONY: experiments/tmi-n
$(tmpi_nodes): experiments/tmi-n%.log: datasets/test.csv out/offline-model.csv bin/tmpi src/evaluation/evaluate.py
	cat out/offline-model.csv datasets/test.csv | $(TIME) mpirun -n $(n) ./bin/tmpi > $(out)-0full.csv 2> $@
	-grep -E -v '^(Unknown|Cluster):' $(out)-0full.csv > $(out)-1matches.csv
	-grep -E '^Unknown:'              $(out)-0full.csv > $(out)-2unknowns.csv
	-grep -E '^Cluster:'              $(out)-0full.csv > $(out)-3clusters.csv
	echo "" >> $@
	-python3 src/evaluation/evaluate.py "Mfog tmi n$(n)" datasets/test.csv $(out)-1matches.csv $@.png >> $@
experiments/rpi/almoco/tmi-n2.log:
	nc -lp 3131 | grep -E -v '^(Unknown|Cluster):' | pv > out/rpi-almoco/tmi-n2.csv &
	$(SSH) almoco "cd ./mfog && cat ./out/offline-model.csv ./datasets/test.csv | $(TIME) mpirun -n 2 ./bin/tmpi | nc -q 0 localhost 3131" 2> $@
	echo "" >> $@
	-python3 src/evaluation/evaluate.py "Cluster tmi n2" datasets/test.csv out/rpi-almoco/tmi-n2.csv $@.png >> $@
#
experiments/big-cluster.log: datasets/test.csv bin/tmpi
	scp almoco:~/mfog/bin/tmpi jantar:~/mfog/bin/
	scp almoco:~/mfog/bin/tmpi lanche:~/mfog/bin/
	echo "" > $@
	for i in {4..12}; do \
		echo "-------- cluster with n $$i --------" ; \
		echo "-------- cluster with n $$i --------" >> $@ ; \
		cat out/offline-model.csv datasets/test.csv \
			| $(TIME) mpirun -n $$i -hostfile ./conf/hostsfile ./bin/tmpi > $(out)-full.csv 2>> $@ ; \
		grep -E -v '^(Unknown|Cluster):' $(out)-full.csv > $(out).csv ; \
		python3 src/evaluation/evaluate.py "Cluster tmi n$$i" datasets/test.csv $(out).csv $@.png >> $@ || true; \
	done
	grep -E '(seconds.)|(system   )|(Hits  )|(Unknowns      )' $@ > $(subst .log,,$@)-simple.log
pi_recoop:
	for i in almoco lanche jantar; do \
		ssh $$i "cd ~/mfog/experiments && tar cz ." | tar xmzf - --directory=experiments/rpi/$$i/ & \
		ssh $$i "cd ~/mfog/out && tar cz ." | tar xmzf - --directory=out/rpi-$$i/ & \
	done

experiments/tmi-supressed.log: out/offline-model.csv datasets/test.csv bin/tmpi
	echo '------ Fastest (no output) ------' > $@
	cat out/offline-model.csv datasets/test.csv | $(TIME) mpirun -n 4 ./bin/tmpi 0 > $(out)4-fastest.csv 2>> $@
	echo '------ Fast (only unk and novelty output) ------' >> $@
	cat out/offline-model.csv datasets/test.csv | $(TIME) mpirun -n 4 ./bin/tmpi 1 > $(out)4-fast.csv 2>> $@
	-python3 src/evaluation/evaluate.py 'Output Only Unk and Novelty' datasets/test.csv $(out)4-fast.csv $@-fast.png >> $@
	echo '------ Full (all labels) ------' >> $@
	cat out/offline-model.csv datasets/test.csv | $(TIME) mpirun -n 4 ./bin/tmpi 2 > $(out)4.csv 2>> $@
	-grep -E -v '^(Unknown|Cluster):' $(out)4.csv > $(out)4-matches.csv
	-python3 src/evaluation/evaluate.py 'Full Output' datasets/test.csv $(out)4-matches.csv $@.png >> $@
#

experiments/tmi-classifiers.log: out/offline-model.csv datasets/test.csv bin/tmpi
	echo "" > $@
	echo "------- Classifers 1 -------" >> $@
	cat out/offline-model.csv datasets/test.csv | $(TIME) mpirun -n 4 ./bin/tmpi 2 1 > $(out).csv 2>> $@
	grep -E -v '^(Unknown|Cluster):' $(out).csv > $(out)-1matches.csv
	python3 src/evaluation/evaluate.py "Mfog Classifer Threads 1" datasets/test.csv $(out)-1matches.csv $@.png >> $@
	echo "------- Classifers 2 -------" >> $@
	cat out/offline-model.csv datasets/test.csv | $(TIME) mpirun -n 4 ./bin/tmpi 2 2 > $(out).csv 2>> $@
	grep -E -v '^(Unknown|Cluster):' $(out).csv > $(out)-1matches.csv
	python3 src/evaluation/evaluate.py "Mfog Classifer Threads 2" datasets/test.csv $(out)-1matches.csv $@.png >> $@
	echo "------- Classifers 3 -------" >> $@
	cat out/offline-model.csv datasets/test.csv | $(TIME) mpirun -n 4 ./bin/tmpi 2 3 > $(out).csv 2>> $@
	grep -E -v '^(Unknown|Cluster):' $(out).csv > $(out)-1matches.csv
	python3 src/evaluation/evaluate.py "Mfog Classifer Threads 3" datasets/test.csv $(out)-1matches.csv $@.png >> $@
	echo "------- Classifers 4 -------" >> $@
	cat out/offline-model.csv datasets/test.csv | $(TIME) mpirun -n 4 ./bin/tmpi 2 4 > $(out).csv 2>> $@
	grep -E -v '^(Unknown|Cluster):' $(out).csv > $(out)-1matches.csv
	python3 src/evaluation/evaluate.py "Mfog Classifer Threads 4" datasets/test.csv $(out)-1matches.csv $@.png >> $@

export tmfmt=[%C %x]\n\t %E \t %S \t %U \t %P; \t %M \t %F \t %R \t %w \t %c \t %W; \t %I \t %O \t %r \t %s \t %k;
export tmftx=[cmd ex]\n\t elapsed \t kernel \t user \t cpu%%; \t max-KB \t pf \t pf-rec \t waits \t switch \t swap; \t ins \t outs \t rcv \t sent \t sigs;
out/fk-base.csv: makefile bin/fk bin/dt bin/zip out/offline-model.csv datasets/test.csv
	printf "$$tmftx\n\n" > experiments/partials.log
	cat datasets/test.csv | /usr/bin/time --format="$$tmfmt" bin/fk out/offline-model.csv > out/fk-base.csv 2>> experiments/partials.log
	printf "$$tmftx\n\n" >> experiments/partials.log
out/dt-base.csv: out/fk-base.csv
	grep -E '^(Cluster|Unknown)' out/fk-base.csv | /usr/bin/time --format="$$tmfmt" bin/dt > out/dt-base.csv 2>> experiments/partials.log
	printf "$$tmftx\n\n" >> experiments/partials.log
out/fk-fullmodel.csv: out/dt-base.csv
	grep -E '^(Cluster)' out/dt-base.csv > out/fk-full-in.csv
	cat out/fk-full-in.csv datasets/test.csv | /usr/bin/time --format="$$tmfmt" bin/fk > out/fk-fullmodel.csv 2>> experiments/partials.log
	printf "$$tmftx\n\n" >> experiments/partials.log
experiments/partials.log: makefile
	printf "# Mario the pipe worker\n\n" >> experiments/partials.log
	cat datasets/test.csv \
		| /usr/bin/time --format="$$tmfmt" bin/fk out/dt.csv | tee out/fk-zip.csv \
		| grep -E '^(Cluster|Unknown)' | bin/zip out/offline-model.csv \
		| /usr/bin/time --format="$$tmfmt" bin/dt | tee out/dt-zip.csv \
		| grep -E '^(Cluster)' > out/dt.csv 2>> experiments/partials.log
	printf "$$tmftx\n\n" >> experiments/partials.log

# -------------------------- Remote Pi Cluster Experiments ---------------------
SSH = ssh -i ./secrets/id_rsa -F ./conf/ssh.config
SCP = scp -i ./secrets/id_rsa -F ./conf/ssh.config
.PHONY: experiments/rpi send
send:
	$(SSH) almoco "if [ ! -d mfog/bin ]; then mkdir -p mfog/bin mfog/out; fi"
	$(SSH) jantar "if [ ! -d mfog/bin ]; then mkdir -p mfog/bin mfog/out; fi"
	$(SSH) lanche "if [ ! -d mfog/bin ]; then mkdir -p mfog/bin mfog/out; fi"
	tar cz datasets | $(SSH) almoco "cd mfog && tar xmvzf - >/dev/null"
experiments/rpi:
	if [ ! -d experiments/rpi ]; then mkdir -p experiments/rpi; fi
	tar cz conf src makefile | $(SSH) almoco "cd mfog && tar xmvzf - >/dev/null"
	$(SSH) almoco "cd mfog && make bin" > experiments/rpi-make-bin.log 2>&1
	$(SCP) almoco:~/mfog/bin/* jantar:~/mfog/bin/
	$(SCP) almoco:~/mfog/bin/* lanche:~/mfog/bin/
	$(SSH) almoco "cd mfog && make @pi_experiments" >> experiments/rpi-make-bin.log 2>&1
	$(SSH) almoco "tar cz ~/mfog/experiments/rpi/*" | tar xmzf - --directory experiments/rpi
.PHONY: @pi_setup @pi_experiments mv@py
@pi_setup:
	if [ ! -d experiments/rpi ]; then mkdir -p experiments/rpi; fi
	if [ ! -d out ]; then mkdir -p out; fi
	$(TIME) mpirun -hostfile ./conf/hostsfile hostname > experiments/rpi/base-time.log 2>&1
@pi_experiments: @pi_setup @pi_experiments/online-nd.log @pi_experiments/tmi-n
	cd experiments && find . -maxdepth 1 -type f -exec mv -t rpi {} \+
@pi_experiments/online-nd.log @pi_experiments/tmi-n: @pi_experiments/%: $(subst @pi_,,$@)
	echo $@ => $(subst @pi_,,$@)
	make $(subst @pi_,,$@)

experiments/rpi/tmi-rpi-n12.log: $(ds) out/reboot/offline.csv bin/tmpi src/evaluation/evaluate.py
	cat out/reboot/offline.csv datasets/test.csv \
		| mpirun -hostfile ./conf/hostsfile ./bin/tmpi \
		> out/reboot/tmi-rpi-n12.csv 2> $@
	-grep -E -v '^(Unknown|Cluster):' out/reboot/tmi-rpi-n12.csv > out/reboot/tmi-rpi-n12-matches.csv
	-grep -E '^Unknown:' out/reboot/tmi-rpi-n12.csv > out/reboot/tmi-rpi-n12-unknowns.csv
	-grep -E '^Cluster:' out/reboot/tmi-rpi-n12.csv > out/reboot/tmi-rpi-n12-clusters.csv
	-python3 src/evaluation/evaluate.py Mfog-Reboot-tmi-rpi-n12 datasets/test.csv out/reboot/tmi-rpi-n12-matches.csv \
		experiments/rpi/tmi-rpi-n12.png >>$@
# experiments/rpi: experiments/rpi/base-time.log experiments/rpi/serial.log experiments/rpi/split.log experiments/rpi/tmi-rpi-n12.log
experiments/rpi/reboot.log: code@almoco
	$(SSH) almoco "cd cloud && make experiments/rpi/tmi-rpi-n12.log" > $@ 2>&1
	$(SSH) almoco "tar cz ~/cloud/experiments/{rpi/tmi-rpi-n12.{log,png},reboot/tmi-n4.{log,png}}" | tar xmzf - experiments/rpi

	# ssh almoco "cat out/reboot/offline.csv datasets/test.csv | mpirun -n 4 ./bin/tmpi \
	# 	> out/reboot/tmi-n4.csv 2> $@mpirun --path /home/pi/cloud/ --host almoco:4,jantar:4,lanche:4 hostname"
	# ssh almoco "cd cloud && mpirun almoco:4 almoco:4"
	# mkdir -p experiments/rpi
	# scp almoco:~/cloud/experiments/* experiments/rpi/

# -------------------------- Experiments ---------------------------------------
# --------- Experiments: Java reference ---------
experiments/reference-java.log: bin/minas/src-minas.jar datasets/training.csv datasets/test.csv
	$(TIME) java -classpath 'bin/minas/src-minas.jar:' br.ufu.noveltydetection.minas.Minas \
		datasets/training.csv datasets/test.csv out/minas-og/ \
		kmeans kmeans \
		2.0 1 10000 100 true | tee -a $@
experiments/reference-java.log.png: experiments/reference-java.log out/minas-og/2020-08-25T12-18-16.272/results
	python3 src/evaluation/evaluate.py Reference-Java datasets/test.csv out/minas-og/2020-08-25T12-18-16.272/results $@ >> experiments/reference-java.log
experiments/reference-java-nf.log: bin/minas/src-minas-mfogFormat.jar datasets/training.csv datasets/test.csv
	$(TIME) java -ea -classpath 'bin/minas/src-minas-mfogFormat.jar:' br.ufu.noveltydetection.minas.MinasOg \
		datasets/training.csv datasets/test.csv out/minas-nf/ \
		kmeans kmeans \
		2.0 lit 10000 100 false false > $@
experiments/reference-java-nf.log.png: experiments/reference-java-nf.log out/minas-nf/2020-10-05T15-55-37.147/results
	python3 src/evaluation/evaluate.py Reference-Java datasets/test.csv out/minas-nf/2020-10-05T15-55-37.147/results $@ >> experiments/reference-java-nf.log
#
