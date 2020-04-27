watch:
	while true; do inotifywait -qre close_write src; mvn clean package; echo "done"; done
build:
	mvn clean package
