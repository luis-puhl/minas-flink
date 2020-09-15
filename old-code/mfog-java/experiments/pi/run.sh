
java -Xms1024m -Xmx1024m \
    -Dlog.file=/home/pi/flink-1.10.0/log/flink-pi-standalonesession-0-almoco.log \
    -Dlog4j.configuration=file:/home/pi/flink-1.10.0/conf/log4j.properties \
    -Dlogback.configurationFile=file:/home/pi/flink-1.10.0/conf/logback.xml -classpath /home/pi/flink-1.10.0/lib/flink-table_2.11-1.10.0.jar:/home/pi/flink-1.10.0/lib/flink-table-blink_2.11-1.10.0.jar:/home/pi/flink-1.10.0/lib/log4j-1.2.17.jar:/home/pi/flink-1.10.0/lib/slf4j-log4j12-1.7.15.jar:/home/pi/flink-1.10.0/lib/flink\
    -dist_2.11-1.10.0.jar::: org.apache.flink.runtime.entrypoint.StandaloneSessionClusterEntrypoint --configDir /home/pi/flink-1.10.0/conf --executionMode cluster

java -XX:+UseG1GC -Xmx536870902 -Xms536870902 -XX:MaxDirectMemorySize=268435458 -XX:MaxMetaspaceSize=100663296 \
    -Dlog.file=/home/pi/flink-1.10.0/log/flink-pi-taskexecutor-0-almoco.log \
    -Dlog4j.configuration=file:/home/pi/flink-1.10.0/conf/log4j.properties \
    -Dlogback.configurationFile=file:/home/pi/flink-1.10.0/conf/logback.xml -classpath /home/pi/flink-1.10.0/lib/flink-table_2.11-1.10.0.jar:/home/pi/flink-1.10.0/lib/flink-table-blink_2.11-1.10.0.jar:/home/pi/flink-1.10.0/lib/log4j-1.2.17.jar:/home/pi/flink-1.10.0/lib/slf4j-log4j12-1.7.15.jar:/home/pi/flink-1.10.0/lib/flink\
    -dist_2.11-1.10.0.jar::: org.apache.flink.runtime.taskexecutor.TaskManagerRunner --configDir /home/pi/flink-1.10.0/conf \
    -D taskmanager.memory.framework.off-heap.size=134217728b \
    -D taskmanager.memory.network.max=134217730b \
    -D taskmanager.memory.network.min=134217730b \
    -D taskmanager.memory.framework.heap.size=134217728b \
    -D taskmanager.memory.managed.size=536870920b \
    -D taskmanager.cpu.cores=4.0 \
    -D taskmanager.memory.task.heap.size=402653174b \
    -D taskmanager.memory.task.off-heap.size=0b