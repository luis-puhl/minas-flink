all: build run kill

watch:
	while true; do mvn clean package; echo "done"; inotifywait -qre close_write src; done
build:
	mvn clean package
run:
	java -cp target/mfog-0.4.jar br.ufscar.dc.gsdr.mfog.ModelStore      & echo $$! > ModelStore.pid ;\
	java -cp target/mfog-0.4.jar br.ufscar.dc.gsdr.mfog.SinkFog         & echo $$! > SinkFog.pid ;\
	java -cp target/mfog-0.4.jar br.ufscar.dc.gsdr.mfog.SourceKyoto     & echo $$! > SourceKyoto.pid ;\
	java -cp target/mfog-0.4.jar br.ufscar.dc.gsdr.mfog.TrainingStatic ;\
	sleep 10 ;\
	../ref-git/flink-1.10.0/bin/flink run target/mfog-0.4.jar --class br.ufscar.dc.gsdr.mfog.Classifier
kill:
	@F="ModelStore.pid";  ([ -f $$F ] && PID=`cat $$F`; ps $$PID > /dev/null && kill -9 $$PID && rm $$F; echo "killed $$PID; rm $$F"; ) || echo ok
	@F="SourceKyoto.pid"; ([ -f $$F ] && PID=`cat $$F`; ps $$PID > /dev/null && kill -9 $$PID && rm $$F; echo "killed $$PID; rm $$F"; ) || echo ok
	@F="SinkFog.pid";     ([ -f $$F ] && PID=`cat $$F`; ps $$PID > /dev/null && kill -9 $$PID && rm $$F; echo "killed $$PID; rm $$F"; ) || echo ok

# Job Runtime: 191963 ms
# 2020-04-28T12:28:39.431 INFO  Classifier Ran baseline in 194.661s
