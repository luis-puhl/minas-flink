build:
	mvn package

kill:
	@-ps -o "%p|%a" | grep "[j]ava -cp target/" | tee /dev/tty | awk -F "|" '{print $$1}' > tokill.ps
	@-while read p; do [ $$p ] && kill $$p; done < tokill.ps
	@-rm tokill.ps

run:
	java -cp target/*:../ref-git/flink-1.10.0/lib/*     br.ufscar.dc.gsdr.mfog.nio.Echo
