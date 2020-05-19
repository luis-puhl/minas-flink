all: kill build run kill

watch:
	while true; do mvn clean package; echo "done"; inotifywait -qre close_write src; done
build:
	mvn clean package

runMfog: kill
	@java -cp target/*:../ref-git/flink-1.10.0/lib/*     br.ufscar.dc.gsdr.mfog.ModelStore      &\
	java -cp target/*:../ref-git/flink-1.10.0/lib/*      br.ufscar.dc.gsdr.mfog.SourceKyoto     &\
	java -cp target/*:../ref-git/flink-1.10.0/lib/*      br.ufscar.dc.gsdr.mfog.SinkFog         &\
	java -cp target/*:../ref-git/flink-1.10.0/lib/*      br.ufscar.dc.gsdr.mfog.TrainingStatic

run: runMfog
	-../ref-git/flink-1.10.0/bin/flink run --class br.ufscar.dc.gsdr.mfog.Classifier target/*
	-@echo done

copyAlmoco:
	scp target/mfog-0.5.jar pi@almoco:/home/pi/
run@almoco: copyAlmoco runMfog
	ssh pi@almoco flink-1.10.0/bin/flink run --class br.ufscar.dc.gsdr.mfog.Classifier ./mfog-0.5.jar
	-@echo done
spot@almoco: copyAlmoco runMfog
	ssh pi@almoco java -cp flink-1.10.0/lib/*:mfog-0.5.jar br.ufscar.dc.gsdr.mfog.Classifier
	-@echo done
cleanAlmoco:
	ssh pi@almoco '/home/pi/flink-1.10.0/bin/stop-cluster.sh'
	scp -r pi@almoco:/home/pi/flink-1.10.0/log/* experiments/pi/flink.log
	ssh pi@almoco 'rm /home/pi/flink-1.10.0/log/*'
	ssh pi@almoco '/home/pi/flink-1.10.0/bin/start-cluster.sh'

spot: kill build runMfog
	java -cp target/*:../ref-git/flink-1.10.0/lib/* br.ufscar.dc.gsdr.mfog.Classifier

kiss: kill
	java -cp target/*:../ref-git/flink-1.10.0/lib/* br.ufscar.dc.gsdr.mfog.kiss.KeepItSimpleStupid servers &
	../ref-git/flink-1.10.0/bin/flink run --class br.ufscar.dc.gsdr.mfog.kiss.KeepItSimpleStupid target/mfog-*.jar

kill:
	@-ps -o "%p|%a" | grep "[j]ava -cp target/" | tee /dev/tty | awk -F "|" '{print $$1}' > tokill.ps
	@-while read p; do [ $$p ] && kill $$p; done < tokill.ps
	@-rm tokill.ps

# Job Runtime: 191963 ms
# 2020-04-28T12:28:39.431 INFO  Classifier Ran baseline in 194.661s
