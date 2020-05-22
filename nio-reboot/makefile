all:
	mvn package

kill:
	@-ps -o "%p|%a" | grep "[j]ava -cp target/" | tee /dev/tty | awk -F "|" '{print $$1}' > tokill.ps
	@-while read p; do [ $$p ] && kill $$p; done < tokill.ps
	@-rm tokill.ps
