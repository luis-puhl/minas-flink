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

.PHONY: bin
bin: bin/offline bin/ond bin/tmpi bin/fk bin/dt bin/zip

# --------------------------------- Experiments --------------------------------
export TIME_FORMAT="%C\n\t%U user\t%S system\t%E elapsed\n\t%P CPU\t(%X avgtext+%D avgdata\t%M maxresident)k\n\t%I inputs+%O outputs\t(%F major+%R minor)pagefaults\t%W swaps\n"
TIME := /usr/bin/time --format="$$TIME_FORMAT" /usr/bin/time --output=experiments/timing.log --append --format="$$TIME_FORMAT"
out/offline-model.csv: datasets/training.csv bin/offline
	-@mkdir -p experiments
	cat datasets/training.csv | $(TIME) ./bin/offline > out/offline-model.csv 2> experiments/offline-model.log

experiments/online-nd.log: out/offline-model.csv datasets/test.csv bin/ond
	-@mkdir -p experiments
	cat out/offline-model.csv datasets/test.csv | $(TIME) ./bin/ond > out/ond-0full.csv 2> $@
	-grep -E -v '^(Unknown|Cluster):' out/ond-0full.csv > out/ond-1matches.csv
	@echo -n "Repeats " | tee -a $@
	@sort out/ond-1matches.csv | uniq --count --repeated --check-chars=20 \
		| tee -a out/ond-1matches-repeats.csv | wc -l | tee -a $@
	-grep -E '^Cluster:' out/ond-0full.csv > out/ond-2model.csv
	-grep -E '^Unknown:' out/ond-0full.csv > out/ond-3unk.csv
	-python3 src/evaluation/evaluate.py "Serial Online-ND" datasets/test.csv out/ond-1matches.csv $@ >> $@
experiments/tmi-base.log: datasets/test.csv out/offline-model.csv bin/tmpi src/evaluation/evaluate.py
	-@mkdir -p experiments
	printf "$$tmftx\n\n" > $@
	cat out/offline-model.csv datasets/test.csv \
		| $(TIME) /usr/bin/time --format="$$tmfmt" mpirun -n 4 ./bin/tmpi 2 2 2>> $@ > out/tmi-full.csv
	grep -E -v '^(Unknown|Cluster):' out/tmi-full.csv > out/tmi-matches.csv
	python3 src/evaluation/evaluate.py "Cluster tmi" datasets/test.csv out/tmi-matches.csv $@ >> $@
	printf "$$tmftx\n" > $(subst .log,,$@)-simple.log
	-grep -E '(-------- cluster)|(./bin/tmpi)|(Hits  )|(Unknowns      )' $@ >> $(subst .log,,$@)-simple.log

# ds = datasets/training.csv datasets/emtpyline datasets/test.csv
# n = $(subst experiments/tmi-n,,$(subst .log,,$@))
out = $(subst experiments/,out/,$(subst .log,,$@))
i = $(subst experiments/speedup-n,,$(subst .log,,$@))
speedup_logs := $(foreach wrd,2 3 4 5 6 7 8 9 10 11 12,experiments/speedup-n$(wrd).log)
$(speedup_logs): bin/tmpi out/offline-model.csv datasets/test.csv
	cat out/offline-model.csv datasets/test.csv \
		| /usr/bin/time mpirun -n $(i) -hostfile ./conf/hostsfile ./bin/tmpi > $(out)-full.csv 2>> $@
	grep -E -v '^(Unknown|Cluster):' $(out)-full.csv > $(out)-matches.csv
	python3 src/evaluation/evaluate.py "Speedup $(i)" datasets/test.csv $(out)-matches.csv $@ >> $@
	# printf "$$tmftx\n" > $(subst .log,,$@)-simple.log
	-grep -E '(-------- cluster)|(./bin/tmpi)|(Hits  )|(Unknowns      )' $@ >> experiments/speedup.log
experiments/speedup.log: $(speedup_logs)
	for f in $^; do grep -E '(-------- cluster)|(./bin/tmpi)|(Hits  )|(Unknowns      )' $$f >> $@; done
# experiments/tmi-n: $(tmpi_nodes)
.PHONY: experiments
experiments: experiments/online-nd.log experiments/tmi-base.log experiments/tmi-MT-classifers/tmi-n.log experiments/tmi-supressed.log
# $(tmpi_nodes): experiments/tmi-n.log: datasets/test.csv out/offline-model.csv bin/tmpi src/evaluation/evaluate.py
export tmfmt=[%C %x]\t %E \t %S \t %U \t %P; \t %M \t %F \t %R \t %w \t %c \t %W; \t %I \t %O \t %r \t %s \t %k;
export tmftx=['cmd' ex]\t elapsed \t kernel \t user \t cpu%%; \t max-KB \t pf \t pf-rec \t waits \t switch \t swap; \t ins \t outs \t rcv \t sent \t sigs;
experiments/tmi-MT-classifers/tmi-n.log: datasets/test.csv out/offline-model.csv bin/tmpi src/evaluation/evaluate.py
	-@mkdir -p experiments/tmi-MT-classifers
	printf "$$tmftx\n\n" > $@
	-for n in {2..4}; do \
		for c in {1..4}; do \
			echo "-------- cluster with n $$n and c $$c --------" | tee -a $@ ; \
			cat out/offline-model.csv datasets/test.csv \
				| /usr/bin/time --format="$$tmfmt" mpirun -n $$n ./bin/tmpi 2 $$c 2>> experiments/tmi-MT-classifers/tmi-n$$n-c$$c.log \
				| tee out/tmi-n$$n-c$$c-full.csv \
				| grep -E -v '^(Unknown|Cluster):' \
				> out/tmi-n$$n-c$$c-matches.csv ; \
			tail $@ | grep "\[./bin/tmpi " ; \
			printf "$$tmftx\n\n" >> $@ ; \
			python3 src/evaluation/evaluate.py "Cluster tmi n$$n c$$c" \
				datasets/test.csv out/tmi-n$$n-c$$c-matches.csv experiments/tmi-MT-classifers/tmi-n$$n-c$$c.log \
				| tee -a $@ | grep "./bin/tmpi " ; \
		done ; \
	done
	printf "$$tmftx\n" > experiments/tmi-MT-classifers/tmi-simple.log
	-grep -E '(-------- cluster)|(./bin/tmpi)|(Hits  )|(Unknowns      )' $@ >> experiments/tmi-MT-classifers/tmi-simple.log
experiments/rpi/almoco/tmi-n2.log:
	nc -lp 3131 | grep -E -v '^(Unknown|Cluster):' | pv > out/rpi-almoco/tmi-n2.csv &
	$(SSH) almoco "cd ./mfog && cat ./out/offline-model.csv ./datasets/test.csv | $(TIME) mpirun -n 2 ./bin/tmpi | nc -q 0 localhost 3131" 2> $@
	echo "" >> $@
	-python3 src/evaluation/evaluate.py "Cluster tmi n2" datasets/test.csv out/rpi-almoco/tmi-n2.csv $@ >> $@
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
		python3 src/evaluation/evaluate.py "Cluster tmi n$$i" datasets/test.csv $(out).csv $@ >> $@ || true; \
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
	-python3 src/evaluation/evaluate.py 'Full Output' datasets/test.csv $(out)4-matches.csv $@ >> $@
#

# out/fk-base.csv: makefile bin/fk bin/dt bin/zip out/offline-model.csv datasets/test.csv
# 	printf "$$tmftx\n\n" > experiments/partials.log
# 	cat datasets/test.csv | /usr/bin/time --format="$$tmfmt" bin/fk out/offline-model.csv > out/fk-base.csv 2>> experiments/partials.log
# 	printf "$$tmftx\n\n" >> experiments/partials.log
# out/dt-base.csv: out/fk-base.csv
# 	grep -E '^(Cluster|Unknown)' out/fk-base.csv | /usr/bin/time --format="$$tmfmt" bin/dt > out/dt-base.csv 2>> experiments/partials.log
# 	printf "$$tmftx\n\n" >> experiments/partials.log
# out/fk-fullmodel.csv: out/dt-base.csv
# 	grep -E '^(Cluster)' out/dt-base.csv > out/fk-full-in.csv
# 	cat out/fk-full-in.csv datasets/test.csv | /usr/bin/time --format="$$tmfmt" bin/fk > out/fk-fullmodel.csv 2>> experiments/partials.log
# 	printf "$$tmftx\n\n" >> experiments/partials.log
# experiments/partials.log: makefile
# 	printf "# Mario the pipe worker\n\n" >> experiments/partials.log
# 	cat datasets/test.csv \
# 		| /usr/bin/time --format="$$tmfmt" bin/fk out/dt.csv | tee out/fk-zip.csv \
# 		| grep -E '^(Cluster|Unknown)' | bin/zip out/offline-model.csv \
# 		| /usr/bin/time --format="$$tmfmt" bin/dt | tee out/dt-zip.csv \
# 		| grep -E '^(Cluster)' > out/dt.csv 2>> experiments/partials.log
# 	printf "$$tmftx\n\n" >> experiments/partials.log

# -------------------------- Remote Pi Cluster Experiments ---------------------
SSH = ssh -F ./conf/ssh.config
SCP = scp -F ./conf/ssh.config
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
	-$(SCP) almoco:~/mfog/bin/* jantar:~/mfog/bin/
	-$(SCP) almoco:~/mfog/bin/* lanche:~/mfog/bin/
	$(SSH) almoco "cd mfog && make @pi_experiments" | tee -a experiments/rpi-make-bin.log 2>&1
	$(SSH) almoco "cd ~/mfog/experiments/ && tar cz ." | tar xmzf - --directory experiments/rpi/almoco
.PHONY: @pi_setup @pi_experiments mv@py
@pi_setup:
	if [ ! -d experiments/rpi ]; then mkdir -p experiments/rpi; fi
	if [ ! -d out ]; then mkdir -p out; fi
	$(TIME) mpirun -hostfile ./conf/hostsfile hostname > experiments/rpi/base-time.log 2>&1
@pi_experiments: @pi_setup experiments
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

experiments/revised-java.log: bin/minas/revised.jar datasets/training.csv datasets/test.csv
	sha1sum bin/minas/revised.jar > $@
	$(TIME) java -ea -classpath 'bin/minas/revised.jar:' NoveltyDetection.MinasRevised \
		datasets/training.csv datasets/test.csv out/revised-java.log true 2>&1 | tee -a $@
	python3 src/evaluation/evaluate.py "Minas Ref. mk1.1" datasets/test.csv out/revised-java.log $@ >> $@
	cp out/revised-java.log-{Literature,Proposed}-chart.png experiments
#
