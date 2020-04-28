watch:
	while true; do mvn clean package; echo "done"; inotifywait -qre close_write src; done
build:
	mvn clean package
run:
	java -cp target/mfog-0.4.jar br.ufscar.dc.gsdr.mfog.ModelStore		& \
	java -cp target/mfog-0.4.jar br.ufscar.dc.gsdr.mfog.SinkFog			& \
	java -cp target/mfog-0.4.jar br.ufscar.dc.gsdr.mfog.SourceKyoto		& \
	java -cp target/mfog-0.4.jar br.ufscar.dc.gsdr.mfog.TrainingStatic	& \
	sleep 10; \
	../ref-git/flink-1.10.0/bin/flink run target/mfog-0.4.jar --class br.ufscar.dc.gsdr.mfog.Classifier
	# Job Runtime: 191963 ms
	# 2020-04-28T12:28:39.431 INFO  Classifier Ran baseline in 194.661s

