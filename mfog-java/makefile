all: kill build run kill

watch:
	while true; do mvn clean package; echo "done"; inotifywait -qre close_write src; done
build:
	mvn clean package

TCP:
	java -cp target/mfog-0.4.jar:../ref-git/flink-1.10.0/lib/flink-dist_2.11-1.10.0.jar br.ufscar.dc.gsdr.mfog.util.TCP server &
	java -cp target/mfog-0.4.jar:../ref-git/flink-1.10.0/lib/flink-dist_2.11-1.10.0.jar br.ufscar.dc.gsdr.mfog.util.TCP
ModelStore:
	java -cp target/mfog-0.4.jar:../ref-git/flink-1.10.0/lib/flink-dist_2.11-1.10.0.jar	br.ufscar.dc.gsdr.mfog.ModelStore &
	java -cp target/mfog-0.4.jar:../ref-git/flink-1.10.0/lib/flink-dist_2.11-1.10.0.jar		br.ufscar.dc.gsdr.mfog.TrainingStatic
runMfog:
	@java -cp target/mfog-0.4.jar:../ref-git/flink-1.10.0/lib/flink-dist_2.11-1.10.0.jar	br.ufscar.dc.gsdr.mfog.ModelStore     &\
	java -cp target/mfog-0.4.jar:../ref-git/flink-1.10.0/lib/flink-dist_2.11-1.10.0.jar		br.ufscar.dc.gsdr.mfog.SinkFog         &\
	java -cp target/mfog-0.4.jar:../ref-git/flink-1.10.0/lib/flink-dist_2.11-1.10.0.jar		br.ufscar.dc.gsdr.mfog.SourceKyoto     &\
	java -cp target/mfog-0.4.jar:../ref-git/flink-1.10.0/lib/flink-dist_2.11-1.10.0.jar		br.ufscar.dc.gsdr.mfog.TrainingStatic

run: runMfog
	-../ref-git/flink-1.10.0/bin/flink run target/mfog-0.4.jar --class br.ufscar.dc.gsdr.mfog.Classifier
	-@echo done

kill:
	@ps -o "%p|%a" | grep "[j]ava -cp target/mfog" | tee /dev/tty | awk -F "|" '{print $$1}' > tokill.ps
	@while read p; do [ $$p ] && kill $$p; done < tokill.ps
	@rm tokill.ps

# Job Runtime: 191963 ms
# 2020-04-28T12:28:39.431 INFO  Classifier Ran baseline in 194.661s
