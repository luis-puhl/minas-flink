watch:
	while true; do mvn clean package; echo "done"; inotifywait -qre close_write src; done
build:
	mvn clean package
